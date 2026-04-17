// Round-trip every supported upstream fixture through the
// registry-backed oracle. This single test exercises:
//
//   - 58-byte header parse + CRC verify
//   - v1 messages (body == content)
//   - v2/v3 messages (extended header + metadata framing)
//   - dispatch via Registry → per-message unpack/pack
//   - byte-exact full-body reassembly
//
// One generic helper handles all cases. To add a new fixture,
// #include the upstream test header and add one fixture() entry.

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

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
#include "oigtl/runtime/error.hpp"
#include "oigtl/runtime/header.hpp"
#include "oigtl/runtime/metadata.hpp"
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

struct Fixture {
    const char* label;
    const std::uint8_t* wire;
    std::size_t wire_len;
    const char* expected_type_id;
};

#define FIXTURE(label, sym, type_id) \
    Fixture{ label, reinterpret_cast<const std::uint8_t*>(sym), \
             sizeof(sym), type_id }

// Some upstream fixtures are split into 2 or 3 separate C arrays
// for human-readability — outer header, child header, child body —
// that the on-the-wire receiver would see as one contiguous stream.
// concat_parts builds a fresh vector each call; the caller stores it
// in named static storage so the buffer outlives the test run.
using BytePart = std::pair<const std::uint8_t*, std::size_t>;

std::vector<std::uint8_t> concat_parts(
    std::initializer_list<BytePart> parts) {
    std::size_t total = 0;
    for (const auto& p : parts) total += p.second;
    std::vector<std::uint8_t> out;
    out.reserve(total);
    for (const auto& p : parts) {
        out.insert(out.end(), p.first, p.first + p.second);
    }
    return out;
}

#define PART(sym) BytePart{ \
    reinterpret_cast<const std::uint8_t*>(sym), sizeof(sym) }

bool run_fixture(const Fixture& f, const oigtl::runtime::Registry& registry) {
    std::printf("[ RUN  ] %s\n", f.label);
    std::fflush(stdout);
    int before = g_fail_count;

    auto result = oigtl::runtime::oracle::verify_wire_bytes(
        f.wire, f.wire_len, registry);
    if (!result.ok) {
        std::fprintf(stderr, "  oracle: %s\n", result.error.c_str());
        ++g_fail_count;
        return false;
    }
    if (result.header.type_id != std::string(f.expected_type_id)) {
        std::fprintf(stderr,
            "  type_id mismatch: got '%s', expected '%s'\n",
            result.header.type_id.c_str(), f.expected_type_id);
    }
    REQUIRE(result.header.type_id == std::string(f.expected_type_id));
    REQUIRE(result.round_trip_ok);

    bool ok = (g_fail_count == before);
    if (ok) std::printf("[  OK  ] %s\n", f.label);
    return ok;
}

// Synthetic round-trip exercising non-empty metadata pack/unpack —
// upstream fixtures don't carry any metadata so the canonical path
// would otherwise be untested.
void test_metadata_round_trip() {
    std::printf("[ RUN  ] metadata round-trip\n");
    std::fflush(stdout);
    int before = g_fail_count;

    std::vector<oigtl::runtime::MetadataEntry> entries;
    {
        oigtl::runtime::MetadataEntry e;
        e.key = "tool_id";
        e.value_encoding = 3;  // US-ASCII
        const std::string v = "needle-42";
        e.value.assign(v.begin(), v.end());
        entries.push_back(std::move(e));
    }
    {
        oigtl::runtime::MetadataEntry e;
        e.key = "operator";
        e.value_encoding = 106;  // UTF-8
        const std::string v = "Dr. \xc3\x96lberg";
        e.value.assign(v.begin(), v.end());
        entries.push_back(std::move(e));
    }

    auto packed = oigtl::runtime::pack_metadata(entries);
    std::vector<std::uint8_t> region;
    region.insert(region.end(),
                  packed.index_bytes.begin(), packed.index_bytes.end());
    region.insert(region.end(),
                  packed.body_bytes.begin(), packed.body_bytes.end());

    auto decoded = oigtl::runtime::unpack_metadata(
        region.data(), region.size(),
        static_cast<std::uint16_t>(packed.index_bytes.size()),
        static_cast<std::uint32_t>(packed.body_bytes.size()));
    REQUIRE(decoded.size() == entries.size());
    for (std::size_t i = 0; i < entries.size(); ++i) {
        REQUIRE(decoded[i].key == entries[i].key);
        REQUIRE(decoded[i].value_encoding == entries[i].value_encoding);
        REQUIRE(decoded[i].value == entries[i].value);
    }

    if (g_fail_count == before) {
        std::printf("[  OK  ] metadata round-trip\n");
    }
}

void test_crc_mismatch_reports_error() {
    std::printf("[ RUN  ] crc mismatch reports error\n");
    std::fflush(stdout);
    int before = g_fail_count;

    auto registry = oigtl::messages::default_registry();
    std::vector<std::uint8_t> tampered(
        test_transform_message,
        test_transform_message + sizeof(test_transform_message));
    tampered[oigtl::runtime::kHeaderSize] ^= 0x01;

    auto result = oigtl::runtime::oracle::verify_wire_bytes(
        tampered.data(), tampered.size(), registry);
    REQUIRE(!result.ok);
    REQUIRE(result.error.find("crc") != std::string::npos
            || result.error.find("CRC") != std::string::npos);

    if (g_fail_count == before) {
        std::printf("[  OK  ] crc mismatch reports error\n");
    }
}

void test_unknown_type_id_reports_error() {
    std::printf("[ RUN  ] unknown type_id reports error\n");
    std::fflush(stdout);
    int before = g_fail_count;

    // Empty registry: every parse will succeed up to dispatch then
    // fail at lookup.
    oigtl::runtime::Registry empty;
    auto result = oigtl::runtime::oracle::verify_wire_bytes(
        reinterpret_cast<const std::uint8_t*>(test_transform_message),
        sizeof(test_transform_message),
        empty);
    REQUIRE(!result.ok);
    REQUIRE(result.error.find("no codec registered") != std::string::npos);

    if (g_fail_count == before) {
        std::printf("[  OK  ] unknown type_id reports error\n");
    }
}

}  // namespace

int main() {
    auto registry = oigtl::messages::default_registry();
    std::printf("Registry contains %zu codecs\n", registry.size());

    // Multi-region fixtures: upstream stores BIND, POLYDATA, NDARRAY
    // as separate C arrays for readability. Reconstruct the wire
    // bytes a real socket would deliver.
    static const std::vector<std::uint8_t> bind_wire = concat_parts({
        PART(test_bind_message_header),
        PART(test_bind_message_bind_header),
        PART(test_bind_message_bind_body),
    });
    static const std::vector<std::uint8_t> polydata_wire = concat_parts({
        PART(test_polydata_message_header),
        PART(test_polydata_message_body),
    });
    static const std::vector<std::uint8_t> ndarray_wire = concat_parts({
        PART(test_ndarray_message_header),
        PART(test_ndarray_message_body),
    });

    // v1 fixtures — body == content, no extended header
    const Fixture v1[] = {
        FIXTURE("TRANSFORM",  test_transform_message,  "TRANSFORM"),
        FIXTURE("STATUS",     test_status_message,     "STATUS"),
        FIXTURE("STRING",     test_string_message,     "STRING"),
        FIXTURE("SENSOR",     test_sensor_message,     "SENSOR"),
        FIXTURE("POSITION",   test_position_message,   "POSITION"),
        FIXTURE("IMAGE",      test_image_message,      "IMAGE"),
        FIXTURE("COLORTABLE", test_colortable_message, "COLORTABLE"),
        FIXTURE("COMMAND",    test_command_message,    "COMMAND"),
        FIXTURE("CAPABILITY", test_capability_message, "CAPABILITY"),
        FIXTURE("POINT",      test_point_message,      "POINT"),
        FIXTURE("TRAJ",       test_trajectory_message, "TRAJ"),
        FIXTURE("TDATA",      test_tdata_message,      "TDATA"),
        FIXTURE("IMGMETA",    test_imgmeta_message,    "IMGMETA"),
        FIXTURE("LBMETA",     test_lbmeta_message,     "LBMETA"),
    };
    for (const auto& f : v1) run_fixture(f, registry);

    // Multi-region fixtures (concatenated above into static storage).
    const Fixture multi[] = {
        Fixture{ "BIND",     bind_wire.data(),     bind_wire.size(),     "BIND" },
        Fixture{ "POLYDATA", polydata_wire.data(), polydata_wire.size(), "POLYDATA" },
        Fixture{ "NDARRAY",  ndarray_wire.data(),  ndarray_wire.size(),  "NDARRAY" },
    };
    for (const auto& f : multi) run_fixture(f, registry);

    // v2/v3 fixtures — extended header + (possibly empty) metadata
    const Fixture v3[] = {
        FIXTURE("TRANSFORM v3", test_transform_message_Format2,   "TRANSFORM"),
        FIXTURE("POSITION v3",  test_position_messageFormat2,     "POSITION"),
        FIXTURE("TDATA v3",     test_tdata_messageFormat2,        "TDATA"),
        FIXTURE("TRAJ v3",      test_trajectory_message_Format2,  "TRAJ"),
        FIXTURE("COMMAND v3",   test_command_messageFormat2,      "COMMAND"),
        FIXTURE("VIDEOMETA v3", test_videometa_message,           "VIDEOMETA"),
    };
    for (const auto& f : v3) run_fixture(f, registry);

    test_metadata_round_trip();
    test_crc_mismatch_reports_error();
    test_unknown_type_id_reports_error();

    if (g_fail_count != 0) {
        std::fprintf(stderr, "\n%d failure(s)\n", g_fail_count);
        return EXIT_FAILURE;
    }
    std::printf("\nAll tests passed.\n");
    return EXIT_SUCCESS;
}
