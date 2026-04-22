// Tests for the public codec + registry API.
//
// Exercises:
//   - Round-trip via unpack_envelope<T> / pack_envelope<T> (owning,
//     raw-bytes form — no Incoming required).
//   - Two-step form unpack_header + unpack_message<T>.
//   - MessageTypeMismatch when T doesn't match the wire's type_id.
//   - Framing validity (truncation, trailing bytes).
//   - Registry: register_message_type / lookup_message_class /
//     unregister_message_type / registered_types, collision
//     detection, idempotent re-register, explicit replace.
//
// Parallels core-py/tests/test_codec.py and
// core-ts/tests/codec.test.ts.

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "oigtl/envelope.hpp"
#include "oigtl/messages/register_all.hpp"
#include "oigtl/messages/status.hpp"
#include "oigtl/messages/transform.hpp"
#include "oigtl/pack.hpp"
#include "oigtl/runtime/dispatch.hpp"
#include "oigtl/runtime/error.hpp"
#include "oigtl/runtime/header.hpp"

namespace om = oigtl::messages;
namespace orr = oigtl::runtime;

namespace {

int g_fail_count = 0;

#define REQUIRE(expr) do {                                            \
    if (!(expr)) {                                                    \
        std::fprintf(stderr, "  FAIL: %s:%d  %s\n",                   \
                     __FILE__, __LINE__, #expr);                      \
        ++g_fail_count;                                               \
    }                                                                 \
} while (0)

#define REQUIRE_THROWS(expr, ExceptionType) do {                      \
    bool threw = false;                                               \
    try { (void)(expr); }                                             \
    catch (const ExceptionType&) { threw = true; }                    \
    catch (...) {                                                     \
        std::fprintf(stderr,                                          \
            "  FAIL: %s:%d  %s threw wrong exception type\n",         \
            __FILE__, __LINE__, #expr);                               \
        ++g_fail_count;                                               \
        threw = true;                                                 \
    }                                                                 \
    if (!threw) {                                                     \
        std::fprintf(stderr,                                          \
            "  FAIL: %s:%d  %s did not throw %s\n",                   \
            __FILE__, __LINE__, #expr, #ExceptionType);               \
        ++g_fail_count;                                               \
    }                                                                 \
} while (0)

// ---------------------------------------------------------------------------
// Round-trip through the raw-bytes form.
// ---------------------------------------------------------------------------

std::vector<std::uint8_t> make_transform_wire() {
    oigtl::Envelope<om::Transform> env;
    env.version = 1;   // v1 keeps body = content, simplest case
    env.device_name = "Probe";
    env.timestamp = 1'700'000'000'000'000'000ULL;
    env.body.matrix = {1, 0, 0, 11,
                       0, 1, 0, 22,
                       0, 0, 1, 33};
    return oigtl::pack_envelope(env);
}

void test_unpack_envelope_from_raw_bytes_round_trips() {
    std::fprintf(stderr, "test_unpack_envelope_from_raw_bytes_round_trips\n");
    const auto wire = make_transform_wire();

    auto env = oigtl::unpack_envelope<om::Transform>(wire);
    REQUIRE(env.device_name == "Probe");
    REQUIRE(env.body.matrix.size() == 12);
    REQUIRE(env.body.matrix[3] == 11.0);
    REQUIRE(env.body.matrix[7] == 22.0);
    REQUIRE(env.body.matrix[11] == 33.0);

    // Re-pack with the original timestamp so pack_envelope doesn't
    // substitute now_igtl() — then byte-compare.
    auto repacked = oigtl::pack_envelope(env);
    REQUIRE(repacked == wire);
}

void test_two_step_matches_one_step() {
    std::fprintf(stderr, "test_two_step_matches_one_step\n");
    const auto wire = make_transform_wire();

    // Two-step: unpack_header → unpack_message.
    auto header = orr::unpack_header(wire.data(), orr::kHeaderSize);
    const std::uint8_t* body = wire.data() + orr::kHeaderSize;
    auto two_step = oigtl::unpack_message<om::Transform>(
        header, body, header.body_size);

    // One-step: unpack_envelope on the full buffer.
    auto one_step = oigtl::unpack_envelope<om::Transform>(wire);

    REQUIRE(two_step.body.matrix == one_step.body.matrix);
    REQUIRE(two_step.device_name == one_step.device_name);
    REQUIRE(two_step.timestamp == one_step.timestamp);
}

void test_type_mismatch_throws() {
    std::fprintf(stderr, "test_type_mismatch_throws\n");
    const auto wire = make_transform_wire();
    // Wire is TRANSFORM; request STATUS.
    REQUIRE_THROWS(
        oigtl::unpack_envelope<om::Status>(wire),
        oigtl::MessageTypeMismatch);
}

void test_truncated_wire_throws() {
    std::fprintf(stderr, "test_truncated_wire_throws\n");
    auto wire = make_transform_wire();
    wire.pop_back();  // drop the final body byte
    REQUIRE_THROWS(
        oigtl::unpack_envelope<om::Transform>(wire),
        oigtl::error::ShortBufferError);
}

void test_trailing_bytes_rejected() {
    std::fprintf(stderr, "test_trailing_bytes_rejected\n");
    auto wire = make_transform_wire();
    wire.push_back(0x99);
    REQUIRE_THROWS(
        oigtl::unpack_envelope<om::Transform>(wire),
        oigtl::error::MalformedMessageError);
}

void test_header_shorter_than_required_throws() {
    std::fprintf(stderr, "test_header_shorter_than_required_throws\n");
    std::vector<std::uint8_t> too_short(orr::kHeaderSize - 1, 0);
    REQUIRE_THROWS(
        oigtl::unpack_envelope<om::Transform>(too_short),
        oigtl::error::ShortBufferError);
}

void test_bad_crc_is_caught() {
    std::fprintf(stderr, "test_bad_crc_is_caught\n");
    auto wire = make_transform_wire();
    // CRC lives in the last 8 bytes of the 58-byte header.
    wire[orr::kHeaderSize - 1] ^= 0xFF;
    REQUIRE_THROWS(
        oigtl::unpack_envelope<om::Transform>(wire),
        oigtl::error::CrcMismatchError);
}

void test_body_length_mismatch_in_unpack_message() {
    std::fprintf(stderr, "test_body_length_mismatch_in_unpack_message\n");
    const auto wire = make_transform_wire();
    auto header = orr::unpack_header(wire.data(), orr::kHeaderSize);
    const std::uint8_t* body = wire.data() + orr::kHeaderSize;
    REQUIRE_THROWS(
        oigtl::unpack_message<om::Transform>(
            header, body, header.body_size - 1),
        oigtl::error::MalformedMessageError);
}

// ---------------------------------------------------------------------------
// Registry.
// ---------------------------------------------------------------------------

orr::Registry make_populated_registry() {
    orr::Registry reg;
    oigtl::messages::register_all(reg);
    return reg;
}

void test_built_ins_are_registered() {
    std::fprintf(stderr, "test_built_ins_are_registered\n");
    auto reg = make_populated_registry();
    REQUIRE(reg.size() == 84);
    REQUIRE(reg.contains("TRANSFORM"));
    REQUIRE(reg.contains("STATUS"));
    REQUIRE(reg.contains("IMAGE"));
    REQUIRE(!reg.contains("NOPE_NOT_A_TYPE"));
}

void test_lookup_returns_round_trip_fn() {
    std::fprintf(stderr, "test_lookup_returns_round_trip_fn\n");
    auto reg = make_populated_registry();
    auto fn = reg.lookup_message_class("TRANSFORM");
    REQUIRE(fn != nullptr);

    // Round-trip a TRANSFORM body through the registered function
    // and check it returns the same bytes (it's unpack-then-repack).
    oigtl::Envelope<om::Transform> env;
    env.version = 1;
    env.body.matrix = {1, 0, 0, 1, 0, 1, 0, 2, 0, 0, 1, 3};
    auto content = env.body.pack();
    auto round_tripped = fn(content.data(), content.size());
    REQUIRE(round_tripped == content);
}

void test_registered_types_is_sorted_and_complete() {
    std::fprintf(stderr, "test_registered_types_is_sorted_and_complete\n");
    auto reg = make_populated_registry();
    auto types = reg.registered_types();
    REQUIRE(types.size() == reg.size());
    // Confirm sorted order.
    for (std::size_t i = 1; i < types.size(); ++i) {
        REQUIRE(types[i - 1] < types[i]);
    }
    // Spot-check a handful.
    bool has_transform = false, has_status = false;
    for (const auto& t : types) {
        if (t == "TRANSFORM") has_transform = true;
        if (t == "STATUS")    has_status = true;
    }
    REQUIRE(has_transform);
    REQUIRE(has_status);
}

// Dummy round-trip fn for collision/replace tests.
std::vector<std::uint8_t> identity_fn(const std::uint8_t* data,
                                      std::size_t length) {
    return std::vector<std::uint8_t>(data, data + length);
}

std::vector<std::uint8_t> identity_fn_alt(const std::uint8_t* data,
                                          std::size_t length) {
    // Functionally the same as identity_fn, but the body is
    // deliberately different so MSVC's /OPT:ICF does not fold
    // the two into a single symbol — ICF would make the
    // `register_message_type(same-key, different-fn)` collision
    // test register the same pointer twice and take the
    // idempotent path, false-passing the test.
    std::vector<std::uint8_t> out;
    out.reserve(length);
    for (std::size_t i = 0; i < length; ++i) out.push_back(data[i]);
    return out;
}

void test_collision_throws_without_replace() {
    std::fprintf(stderr, "test_collision_throws_without_replace\n");
    orr::Registry reg;
    reg.register_message_type("EXT_TEST", &identity_fn);
    REQUIRE_THROWS(
        reg.register_message_type("EXT_TEST", &identity_fn_alt),
        orr::RegistryConflictError);
}

void test_idempotent_on_same_fn() {
    std::fprintf(stderr, "test_idempotent_on_same_fn\n");
    orr::Registry reg;
    reg.register_message_type("EXT_TEST", &identity_fn);
    reg.register_message_type("EXT_TEST", &identity_fn);  // no throw
    REQUIRE(reg.lookup_message_class("EXT_TEST") == &identity_fn);
}

void test_replace_overwrites() {
    std::fprintf(stderr, "test_replace_overwrites\n");
    orr::Registry reg;
    reg.register_message_type("EXT_TEST", &identity_fn);
    reg.register_message_type("EXT_TEST", &identity_fn_alt, /*replace=*/true);
    REQUIRE(reg.lookup_message_class("EXT_TEST") == &identity_fn_alt);
}

void test_built_in_collision_is_protected() {
    std::fprintf(stderr, "test_built_in_collision_is_protected\n");
    auto reg = make_populated_registry();
    REQUIRE_THROWS(
        reg.register_message_type("TRANSFORM", &identity_fn),
        orr::RegistryConflictError);
}

void test_unregister_returns_prior_fn() {
    std::fprintf(stderr, "test_unregister_returns_prior_fn\n");
    orr::Registry reg;
    reg.register_message_type("EXT_TEST", &identity_fn);
    auto prior = reg.unregister_message_type("EXT_TEST");
    REQUIRE(prior == &identity_fn);
    REQUIRE(reg.lookup_message_class("EXT_TEST") == nullptr);
}

void test_unregister_missing_is_nullptr() {
    std::fprintf(stderr, "test_unregister_missing_is_nullptr\n");
    orr::Registry reg;
    REQUIRE(reg.unregister_message_type("NEVER_REGISTERED") == nullptr);
}

// ---------------------------------------------------------------------------
// Extension decode through the pack/unpack API.
//
// C++ is compile-time typed — unlike Python/TS, the codec can't
// return `Envelope<unknown>`. But a third-party message class that
// satisfies the same contract as built-ins (kTypeId, pack(),
// static unpack()) decodes through the same unpack_envelope<T>
// path. This test defines such a class and verifies round-trip.
// ---------------------------------------------------------------------------

struct FakeExtension {
    static constexpr const char* kTypeId = "EXT_FAKE";
    std::uint32_t value = 0;

    std::vector<std::uint8_t> pack() const {
        std::vector<std::uint8_t> out(4);
        // Little-endian encoding — arbitrary choice, matches the
        // parallel TS test.
        out[0] = static_cast<std::uint8_t>(value & 0xff);
        out[1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
        out[2] = static_cast<std::uint8_t>((value >> 16) & 0xff);
        out[3] = static_cast<std::uint8_t>((value >> 24) & 0xff);
        return out;
    }

    static FakeExtension unpack(const std::uint8_t* data,
                                std::size_t length) {
        if (length != 4) {
            throw oigtl::error::MalformedMessageError(
                "FakeExtension expects 4 bytes");
        }
        FakeExtension v;
        v.value =
            static_cast<std::uint32_t>(data[0]) |
            (static_cast<std::uint32_t>(data[1]) << 8) |
            (static_cast<std::uint32_t>(data[2]) << 16) |
            (static_cast<std::uint32_t>(data[3]) << 24);
        return v;
    }
};

void test_extension_decodes_through_same_api() {
    std::fprintf(stderr, "test_extension_decodes_through_same_api\n");
    oigtl::Envelope<FakeExtension> env;
    env.version = 1;
    env.device_name = "vendor";
    env.timestamp = 1'700'000'000ULL << 32;
    env.body.value = 0xCAFEBABE;

    auto wire = oigtl::pack_envelope(env);
    auto decoded = oigtl::unpack_envelope<FakeExtension>(wire);

    REQUIRE(decoded.body.value == 0xCAFEBABE);
    REQUIRE(decoded.device_name == "vendor");

    auto repacked = oigtl::pack_envelope(decoded);
    REQUIRE(repacked == wire);
}

}  // namespace

int main() {
    test_unpack_envelope_from_raw_bytes_round_trips();
    test_two_step_matches_one_step();
    test_type_mismatch_throws();
    test_truncated_wire_throws();
    test_trailing_bytes_rejected();
    test_header_shorter_than_required_throws();
    test_bad_crc_is_caught();
    test_body_length_mismatch_in_unpack_message();
    test_built_ins_are_registered();
    test_lookup_returns_round_trip_fn();
    test_registered_types_is_sorted_and_complete();
    test_collision_throws_without_replace();
    test_idempotent_on_same_fn();
    test_replace_overwrites();
    test_built_in_collision_is_protected();
    test_unregister_returns_prior_fn();
    test_unregister_missing_is_nullptr();
    test_extension_decodes_through_same_api();

    if (g_fail_count > 0) {
        std::fprintf(stderr,
                     "\ncodec_test: %d FAIL(s)\n", g_fail_count);
        return 1;
    }
    std::fprintf(stderr, "\ncodec_test: all OK\n");
    return 0;
}
