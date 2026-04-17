// Cross-language oracle parity: for every upstream fixture, run
// both the C++ oracle and the Python oracle (via the
// `oigtl-corpus oracle verify --fixture <name>` CLI) and assert
// they agree on:
//
//   - ok flag
//   - reported wire type_id
//   - declared protocol version
//   - body size
//   - extended-header presence + size
//   - number of metadata entries decoded
//   - round-trip success
//
// Both implementations independently load their own copy of the
// canonical upstream `.h` fixture bytes, so a divergence here
// proves the implementations themselves disagree (not that the
// inputs differ). This is mostly belt-and-suspenders coverage —
// the per-implementation tests already prove byte-exact round-trip
// against the same fixtures — but a parity break would still
// flag a real bug in either codec.
//
// Build- and run-time requirements:
//   * `uv` and the `oigtl-corpus` console script must be
//     available on PATH; the test reports "skip" if not. CI sets
//     this up; local dev usually does too.
//   * popen()/pclose() (POSIX). Windows would need a _popen
//     fallback or a separate process-spawn helper — out of scope
//     for now.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#  include <io.h>
#else
#  include <unistd.h>
#endif

extern "C" {
#include "igtl_test_data_bind.h"
#include "igtl_test_data_capability.h"
#include "igtl_test_data_colortable.h"
#include "igtl_test_data_command.h"
#include "igtl_test_data_commandFormat2.h"
#include "igtl_test_data_image.h"
#include "igtl_test_data_imgmeta.h"
#include "igtl_test_data_lbmeta.h"
#include "igtl_test_data_ndarray.h"
#include "igtl_test_data_point.h"
#include "igtl_test_data_polydata.h"
#include "igtl_test_data_position.h"
#include "igtl_test_data_positionFormat2.h"
#include "igtl_test_data_sensor.h"
#include "igtl_test_data_status.h"
#include "igtl_test_data_string.h"
#include "igtl_test_data_tdata.h"
#include "igtl_test_data_tdataFormat2.h"
#include "igtl_test_data_trajectory.h"
#include "igtl_test_data_trajectoryFormat2.h"
#include "igtl_test_data_transform.h"
#include "igtl_test_data_transformFormat2.h"
#include "igtl_test_data_videometa.h"
}

#include "oigtl/messages/register_all.hpp"
#include "oigtl/runtime/oracle.hpp"

namespace {

int g_fail_count = 0;

#define REQUIRE(expr) do {                                           \
    if (!(expr)) {                                                   \
        std::fprintf(stderr,                                         \
            "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);        \
        ++g_fail_count;                                              \
    }                                                                \
} while (0)

// One canonical wire blob, identified by both:
//   - C++ test_*[] symbol (already #include'd above)
//   - Python fixture name (key in UPSTREAM_VECTORS)
//
// For multi-region fixtures, parts is non-empty and the C++ side
// concatenates them; the Python side already exposes the
// concatenated form via its own loader.
struct ParityCase {
    const char* python_fixture_name;
    const std::uint8_t* contiguous_wire;   // null for multi-region
    std::size_t contiguous_size;
    // Multi-region: ordered (ptr, len) pairs to concat.
    std::vector<std::pair<const std::uint8_t*, std::size_t>> parts;
};

#define SINGLE(name, sym) ParityCase{                                  \
    name, reinterpret_cast<const std::uint8_t*>(sym), sizeof(sym), {} }

#define MULTI(name, ...) ParityCase{                                   \
    name, nullptr, 0, { __VA_ARGS__ } }
#define P(sym) std::pair<const std::uint8_t*, std::size_t>{            \
    reinterpret_cast<const std::uint8_t*>(sym), sizeof(sym) }

std::vector<std::uint8_t> assemble(const ParityCase& c) {
    if (c.contiguous_wire != nullptr) {
        return std::vector<std::uint8_t>(
            c.contiguous_wire, c.contiguous_wire + c.contiguous_size);
    }
    std::vector<std::uint8_t> out;
    std::size_t total = 0;
    for (const auto& p : c.parts) total += p.second;
    out.reserve(total);
    for (const auto& p : c.parts) {
        out.insert(out.end(), p.first, p.first + p.second);
    }
    return out;
}

// Trivial JSON value extractor — matches "key": value (bool, int,
// quoted string, or null) on a flat one-key-per-line document of
// the shape our Python `oracle verify` emits. Doesn't handle
// arbitrary JSON; that would be over-engineered for this test.
struct JsonReport {
    std::unordered_map<std::string, std::string> values;

    std::string str(const char* key) const {
        auto it = values.find(key);
        return (it == values.end()) ? std::string{} : it->second;
    }
    bool boolean(const char* key) const { return str(key) == "true"; }
    long integer(const char* key) const {
        const std::string& s = str(key);
        if (s.empty() || s == "null") return 0;
        return std::strtol(s.c_str(), nullptr, 10);
    }
    bool is_null(const char* key) const { return str(key) == "null"; }
};

JsonReport parse_flat_json(const std::string& text) {
    JsonReport report;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        // Format: optional whitespace, "key": value (with optional
        // trailing comma).
        std::size_t qstart = line.find('"');
        if (qstart == std::string::npos) continue;
        std::size_t qend = line.find('"', qstart + 1);
        if (qend == std::string::npos) continue;
        std::size_t colon = line.find(':', qend);
        if (colon == std::string::npos) continue;
        std::string key = line.substr(qstart + 1, qend - qstart - 1);
        std::string val = line.substr(colon + 1);
        // Strip leading whitespace.
        std::size_t i = 0;
        while (i < val.size()
               && (val[i] == ' ' || val[i] == '\t')) ++i;
        val.erase(0, i);
        // Strip trailing whitespace + comma.
        while (!val.empty()
               && (val.back() == ',' || val.back() == ' '
                   || val.back() == '\t' || val.back() == '\r'
                   || val.back() == '\n')) {
            val.pop_back();
        }
        // Strip surrounding quotes for string values.
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
            val = val.substr(1, val.size() - 2);
        }
        report.values.emplace(std::move(key), std::move(val));
    }
    return report;
}

// Returns (exit_code, stdout). Returns (-1, "") on spawn failure.
std::pair<int, std::string> run_command(const std::string& cmd) {
#if defined(_WIN32)
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return {-1, ""};
    std::string output;
    char buf[4096];
    while (std::size_t n = std::fread(buf, 1, sizeof(buf), pipe)) {
        output.append(buf, n);
    }
#if defined(_WIN32)
    int rc = _pclose(pipe);
#else
    int rc = pclose(pipe);
#endif
    return {rc, std::move(output)};
}

bool oigtl_corpus_available() {
    auto [rc, out] = run_command("uv run oigtl-corpus --version 2>/dev/null");
    return rc == 0;
}

void compare(const ParityCase& c) {
    std::printf("[ RUN  ] parity %s\n", c.python_fixture_name);
    std::fflush(stdout);
    int before = g_fail_count;

    // --- Python side ---
    std::ostringstream cmd;
    cmd << "uv run oigtl-corpus oracle verify --fixture "
        << c.python_fixture_name;
    auto [rc, stdout_text] = run_command(cmd.str());
    // rc != 0 just means ok=false; we still parse and compare.
    auto py = parse_flat_json(stdout_text);
    if (py.values.empty()) {
        std::fprintf(stderr,
            "  python verify produced no parseable output (rc=%d):\n%s\n",
            rc, stdout_text.c_str());
        ++g_fail_count;
        return;
    }

    // --- C++ side ---
    auto registry = oigtl::messages::default_registry();
    auto wire = assemble(c);
    auto cpp = oigtl::runtime::oracle::verify_wire_bytes(
        wire.data(), wire.size(), registry);

    // --- Compare ---
    REQUIRE(cpp.ok == py.boolean("ok"));
    REQUIRE(cpp.header.type_id == py.str("type_id"));
    REQUIRE(cpp.header.device_name == py.str("device_name"));
    REQUIRE(static_cast<long>(cpp.header.version) == py.integer("version"));
    REQUIRE(static_cast<long>(cpp.header.body_size)
            == py.integer("body_size"));
    if (cpp.extended_header.has_value()) {
        REQUIRE(!py.is_null("ext_header_size"));
        REQUIRE(static_cast<long>(cpp.extended_header->ext_header_size)
                == py.integer("ext_header_size"));
    } else {
        REQUIRE(py.is_null("ext_header_size"));
    }
    REQUIRE(static_cast<long>(cpp.metadata.size())
            == py.integer("metadata_count"));
    REQUIRE(cpp.round_trip_ok == py.boolean("round_trip_ok"));

    if (g_fail_count == before) {
        std::printf("[  OK  ] parity %s\n", c.python_fixture_name);
    }
}

}  // namespace

int main() {
    if (!oigtl_corpus_available()) {
        std::printf(
            "SKIP: 'uv run oigtl-corpus' not available on PATH.\n"
            "Install corpus-tools (e.g. 'uv sync --project corpus-tools')\n"
            "and ensure 'uv' is on PATH to run this test.\n");
        return 0;
    }

    // Mirrors the fixture set in upstream_fixtures_test.cpp,
    // including the multi-region BIND/POLYDATA/NDARRAY fixtures.
    const ParityCase cases[] = {
        // v1 contiguous
        SINGLE("transform",  test_transform_message),
        SINGLE("status",     test_status_message),
        SINGLE("string",     test_string_message),
        SINGLE("sensor",     test_sensor_message),
        SINGLE("position",   test_position_message),
        SINGLE("image",      test_image_message),
        SINGLE("colortable", test_colortable_message),
        SINGLE("command",    test_command_message),
        SINGLE("capability", test_capability_message),
        SINGLE("point",      test_point_message),
        SINGLE("trajectory", test_trajectory_message),
        SINGLE("tdata",      test_tdata_message),
        SINGLE("imgmeta",    test_imgmeta_message),
        SINGLE("lbmeta",     test_lbmeta_message),

        // v3 contiguous
        SINGLE("transformFormat2",  test_transform_message_Format2),
        SINGLE("positionFormat2",   test_position_messageFormat2),
        SINGLE("tdataFormat2",      test_tdata_messageFormat2),
        SINGLE("trajectoryFormat2", test_trajectory_message_Format2),
        SINGLE("commandFormat2",    test_command_messageFormat2),
        SINGLE("videometa",         test_videometa_message),

        // Multi-region
        MULTI("bind",
              P(test_bind_message_header),
              P(test_bind_message_bind_header),
              P(test_bind_message_bind_body)),
        MULTI("polydata",
              P(test_polydata_message_header),
              P(test_polydata_message_body)),
        MULTI("ndarray",
              P(test_ndarray_message_header),
              P(test_ndarray_message_body)),
    };

    for (const auto& c : cases) compare(c);

    if (g_fail_count != 0) {
        std::fprintf(stderr, "\n%d parity failure(s)\n", g_fail_count);
        return EXIT_FAILURE;
    }
    std::printf("\nAll %zu fixtures agree across C++ and Python oracles.\n",
                sizeof(cases) / sizeof(cases[0]));
    return EXIT_SUCCESS;
}
