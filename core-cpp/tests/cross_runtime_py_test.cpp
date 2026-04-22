// Cross-runtime integration — core-cpp Client ↔ core-py Server.
//
// The complement to core-py/tests/test_cross_runtime_cpp.py: that
// Python test proves core-py Client → core-cpp Server works; this
// C++ test proves core-cpp Client → core-py Server works.
//
// Spawns `uv run python core-cpp/tests/fixtures/python_tcp_echo.py`
// as a subprocess, reads its "PORT=<n>\n" line, then connects a
// core-cpp Client, sends a TRANSFORM, and asserts the STATUS reply
// carries the translation fields we put on the wire.
//
// Skip policy:
//   - Windows: skipped. asyncio's add_signal_handler isn't supported
//     on Windows; the existing python_ws_echo fixture handles this
//     and the pattern carries. More importantly: POSIX-only spawn
//     plumbing (fork/exec/pipe) keeps this test file small and
//     auditable. Windows CI for core-cpp doesn't build upstream either,
//     so matching the existing "compat tests self-gate" posture here
//     is consistent.
//   - Missing `uv` binary: skipped with a clear stderr note. Local
//     devs who haven't installed uv see `#cross_runtime_py exit=0`
//     and a "skipped" line instead of a cryptic spawn failure.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifndef _WIN32
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <chrono>

#include "oigtl/client.hpp"
#include "oigtl/envelope.hpp"
#include "oigtl/messages/status.hpp"
#include "oigtl/messages/transform.hpp"

namespace om = oigtl::messages;

namespace {

int g_fail_count = 0;
#define REQUIRE(expr) do {                                            \
    if (!(expr)) {                                                    \
        std::fprintf(stderr, "  FAIL: %s:%d  %s\n",                   \
                     __FILE__, __LINE__, #expr);                      \
        ++g_fail_count;                                               \
    }                                                                 \
} while (0)

#ifndef _WIN32

// POSIX spawn + pipe to read the fixture's stdout. Returns the
// assigned port, or -1 on failure; *out_pid receives the child
// pid so the caller can clean it up.
//
// The fixture is launched via `uv run python <path>` from the
// core-py working directory so uv picks up that project's
// dependencies.
int spawn_python_fixture(const std::string& core_py_dir,
                         const std::string& fixture_path,
                         pid_t* out_pid) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        std::fprintf(stderr, "pipe() failed: %s\n", std::strerror(errno));
        return -1;
    }

    const pid_t pid = fork();
    if (pid < 0) {
        std::fprintf(stderr, "fork() failed: %s\n", std::strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        // child — replace stdout with the pipe's write end
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
            _exit(127);
        }
        close(pipefd[1]);
        // Child stderr goes to the test's stderr so failures are
        // visible. Change working directory so uv picks up the
        // core-py project's dependencies.
        if (chdir(core_py_dir.c_str()) != 0) {
            std::fprintf(stderr, "chdir(%s) failed: %s\n",
                         core_py_dir.c_str(), std::strerror(errno));
            _exit(127);
        }
        const char* argv[] = {"uv", "run", "python",
                              fixture_path.c_str(), nullptr};
        execvp("uv", const_cast<char* const*>(argv));
        // exec returns only on failure.
        std::fprintf(stderr, "execvp(uv) failed: %s\n",
                     std::strerror(errno));
        _exit(127);
    }

    // parent — read the PORT line from the pipe's read end
    close(pipefd[1]);

    std::string buf;
    char chunk[256];
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(15);
    int port = -1;
    while (std::chrono::steady_clock::now() < deadline) {
        const ssize_t n = read(pipefd[0], chunk, sizeof(chunk));
        if (n <= 0) {
            // EOF or error — the fixture probably died before
            // emitting PORT. Give it a moment for stderr to
            // surface, then give up.
            break;
        }
        buf.append(chunk, static_cast<std::size_t>(n));
        const auto nl = buf.find('\n');
        if (nl != std::string::npos) {
            const std::string line = buf.substr(0, nl);
            if (line.rfind("PORT=", 0) == 0) {
                port = std::atoi(line.c_str() + 5);
                break;
            }
            // Unrelated log line; discard and continue reading.
            buf.erase(0, nl + 1);
        }
    }
    close(pipefd[0]);
    *out_pid = pid;
    return port;
}

void stop_fixture(pid_t pid) {
    if (pid <= 0) return;
    kill(pid, SIGTERM);
    // Give 3 seconds for graceful exit, then SIGKILL.
    for (int i = 0; i < 30; ++i) {
        int status = 0;
        const pid_t done = waitpid(pid, &status, WNOHANG);
        if (done == pid) return;
        usleep(100'000);  // 100 ms
    }
    kill(pid, SIGKILL);
    int status = 0;
    waitpid(pid, &status, 0);
}

// Walk upward from argv[0]'s directory looking for a `core-py`
// sibling that contains a `pyproject.toml` — the same robust
// resolution the Python-side test uses. Returns empty on failure.
std::string find_core_py_dir(const char* argv0) {
    std::string here = argv0 ? argv0 : "./test";
    const auto lastSlash = here.find_last_of('/');
    std::string dir = (lastSlash == std::string::npos)
        ? std::string(".")
        : here.substr(0, lastSlash);
    for (int i = 0; i < 8; ++i) {
        const std::string candidate = dir + "/core-py/pyproject.toml";
        FILE* f = std::fopen(candidate.c_str(), "r");
        if (f != nullptr) {
            std::fclose(f);
            return dir + "/core-py";
        }
        const auto sep = dir.find_last_of('/');
        if (sep == std::string::npos || dir == "/") break;
        dir = dir.substr(0, sep);
        if (dir.empty()) dir = "/";
    }
    // One more try: the build is usually in <repo>/core-cpp/build,
    // and argv[0] may be a bare filename. Try absolute reconstruction
    // from the current working directory.
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        std::string d = cwd;
        for (int i = 0; i < 8; ++i) {
            const std::string candidate = d + "/core-py/pyproject.toml";
            FILE* f = std::fopen(candidate.c_str(), "r");
            if (f != nullptr) {
                std::fclose(f);
                return d + "/core-py";
            }
            const auto sep = d.find_last_of('/');
            if (sep == std::string::npos || d == "/") break;
            d = d.substr(0, sep);
            if (d.empty()) d = "/";
        }
    }
    return "";
}

bool uv_available() {
    // Cheap check: `uv --version` exits 0 if uv is on PATH.
    const int rc = std::system("uv --version >/dev/null 2>&1");
    return rc == 0;
}

void run_test(int argc, char** argv) {
    (void)argc;
    const std::string core_py = find_core_py_dir(argv[0]);
    if (core_py.empty()) {
        std::fprintf(stderr,
            "skip: could not locate core-py sibling directory\n");
        return;
    }
    if (!uv_available()) {
        std::fprintf(stderr,
            "skip: `uv` binary not on PATH — install uv to run\n");
        return;
    }

    // Absolute path so the child's chdir doesn't affect resolution.
    // The fixture lives next to this test source.
    const std::string fixture =
        core_py + "/../core-cpp/tests/fixtures/python_tcp_echo.py";

    pid_t pid = 0;
    const int port = spawn_python_fixture(core_py, fixture, &pid);
    if (port <= 0) {
        std::fprintf(stderr,
            "FAIL: fixture didn't emit PORT within 15s\n");
        ++g_fail_count;
        stop_fixture(pid);
        return;
    }

    try {
        oigtl::ClientOptions opts;
        opts.connect_timeout = std::chrono::seconds(5);
        opts.default_device = "cpp-client";
        auto client = oigtl::Client::connect("127.0.0.1",
                                             static_cast<std::uint16_t>(port),
                                             opts);

        // Send a TRANSFORM with translation values the regex will
        // pick up in the reply's status_message.
        oigtl::Envelope<om::Transform> env;
        env.version = 1;
        env.device_name = "cpp-client";
        env.timestamp = oigtl::now_igtl();
        env.body.matrix = {
            1.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 1.0f,
            11.0f, 22.0f, 33.0f,
        };
        client.send(env);

        auto reply = client.receive<om::Status>();
        REQUIRE(reply.body.code == 1);
        REQUIRE(reply.body.status_message.find(
            "matrix[-3:]=[11.0, 22.0, 33.0]") != std::string::npos);
        std::fprintf(stderr, "round-trip OK: status=%s\n",
                     reply.body.status_message.c_str());
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        ++g_fail_count;
    }

    stop_fixture(pid);
}

#endif  // !_WIN32

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    (void)argc; (void)argv;
    std::fprintf(stderr,
        "skip: cross-runtime py test is POSIX-only (asyncio signals "
        "+ fork/exec)\n");
    return 0;
#else
    run_test(argc, argv);
    if (g_fail_count > 0) {
        std::fprintf(stderr, "\ncross_runtime_py: %d FAIL(s)\n",
                     g_fail_count);
        return 1;
    }
    std::fprintf(stderr, "\ncross_runtime_py: all OK\n");
    return 0;
#endif
}
