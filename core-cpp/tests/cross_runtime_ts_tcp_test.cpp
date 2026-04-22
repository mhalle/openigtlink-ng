// Cross-runtime integration — core-cpp Client ↔ core-ts Server (TCP).
//
// Parallel to cross_runtime_py_test.cpp but drives the TS fixture
// instead of the Python one. Spawns
//     node <repo>/core-ts/build-tests/tests/net/fixtures/ts_tcp_echo.js
// as a subprocess, reads its "PORT=<n>\n" line, connects a core-cpp
// Client, round-trips a TRANSFORM, asserts the STATUS reply matches.
//
// Closes the cpp → ts-tcp cell in the interop matrix. (The cpp
// → ts-ws cell stays uncovered because core-cpp has no WsClient.)
//
// Skip policy:
//   - Windows: skipped. POSIX-only spawn plumbing (fork/exec/pipe)
//     and matching the existing compat-tests-self-gate posture.
//   - Missing `node` binary: skipped with a clear stderr note.
//   - Missing compiled fixture: skipped likewise.

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

// Spawn `node <fixture>` with its stdout piped to us. Returns the
// assigned port, or -1 on failure. *out_pid receives the child pid.
int spawn_ts_fixture(const std::string& fixture_path, pid_t* out_pid) {
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
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) _exit(127);
        close(pipefd[1]);
        const char* argv[] = {"node", fixture_path.c_str(), nullptr};
        execvp("node", const_cast<char* const*>(argv));
        std::fprintf(stderr, "execvp(node) failed: %s\n",
                     std::strerror(errno));
        _exit(127);
    }

    close(pipefd[1]);

    std::string buf;
    char chunk[256];
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(15);
    int port = -1;
    while (std::chrono::steady_clock::now() < deadline) {
        const ssize_t n = read(pipefd[0], chunk, sizeof(chunk));
        if (n <= 0) break;
        buf.append(chunk, static_cast<std::size_t>(n));
        const auto nl = buf.find('\n');
        if (nl != std::string::npos) {
            const std::string line = buf.substr(0, nl);
            if (line.rfind("PORT=", 0) == 0) {
                port = std::atoi(line.c_str() + 5);
                break;
            }
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
    for (int i = 0; i < 30; ++i) {
        int status = 0;
        if (waitpid(pid, &status, WNOHANG) == pid) return;
        usleep(100'000);
    }
    kill(pid, SIGKILL);
    int status = 0;
    waitpid(pid, &status, 0);
}

// Walk upward from argv[0]'s directory looking for the compiled
// TS fixture at <ancestor>/core-ts/build-tests/tests/net/fixtures/
// ts_tcp_echo.js. Returns empty on failure.
std::string find_ts_tcp_echo(const char* argv0) {
    const std::string relative =
        "core-ts/build-tests/tests/net/fixtures/ts_tcp_echo.js";

    auto check_ancestors = [&](std::string dir) -> std::string {
        for (int i = 0; i < 8; ++i) {
            const std::string candidate = dir + "/" + relative;
            FILE* f = std::fopen(candidate.c_str(), "r");
            if (f != nullptr) {
                std::fclose(f);
                return candidate;
            }
            const auto sep = dir.find_last_of('/');
            if (sep == std::string::npos || dir == "/") break;
            dir = dir.substr(0, sep);
            if (dir.empty()) dir = "/";
        }
        return std::string();
    };

    if (argv0 != nullptr) {
        const std::string here = argv0;
        const auto lastSlash = here.find_last_of('/');
        const std::string dir =
            (lastSlash == std::string::npos) ? "." : here.substr(0, lastSlash);
        const auto found = check_ancestors(dir);
        if (!found.empty()) return found;
    }
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        return check_ancestors(cwd);
    }
    return std::string();
}

bool node_available() {
    return std::system("node --version >/dev/null 2>&1") == 0;
}

void run_test(int /*argc*/, char** argv) {
    const std::string fixture = find_ts_tcp_echo(argv[0]);
    if (fixture.empty()) {
        std::fprintf(stderr,
            "skip: core-ts fixture ts_tcp_echo.js not found — run "
            "`npm test` in core-ts to build it\n");
        return;
    }
    if (!node_available()) {
        std::fprintf(stderr,
            "skip: `node` binary not on PATH — install Node.js to run\n");
        return;
    }

    pid_t pid = 0;
    const int port = spawn_ts_fixture(fixture, &pid);
    if (port <= 0) {
        std::fprintf(stderr,
            "FAIL: ts_tcp_echo didn't emit PORT within 15s\n");
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
        "skip: cross-runtime ts-tcp test is POSIX-only\n");
    return 0;
#else
    run_test(argc, argv);
    if (g_fail_count > 0) {
        std::fprintf(stderr, "\ncross_runtime_ts_tcp: %d FAIL(s)\n",
                     g_fail_count);
        return 1;
    }
    std::fprintf(stderr, "\ncross_runtime_ts_tcp: all OK\n");
    return 0;
#endif
}
