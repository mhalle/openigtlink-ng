// Regression test: V3 framer must reject a header whose body_size
// would overflow size_t when added to kHeaderSize.
//
// Before the fix, `kHeaderSize + body_size` wrapped on size_t when
// body_size was near UINT64_MAX, passed the short-frame check, and
// drove out-of-range iterator arithmetic on the input buffer.
//
// We construct a valid 58-byte header by hand (correct CRC for an
// empty body is *not* what we want here — we want to trip the
// overflow guard *before* CRC verification runs, so the CRC field
// can be anything). `try_parse` should throw FramingError.

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/header.hpp"
#include "oigtl/transport/errors.hpp"
#include "oigtl/transport/framer.hpp"

namespace {

int g_fail_count = 0;

#define REQUIRE(expr) do {                                            \
    if (!(expr)) {                                                    \
        std::fprintf(stderr, "  FAIL: %s:%d  %s\n",                   \
                     __FILE__, __LINE__, #expr);                      \
        ++g_fail_count;                                               \
    }                                                                 \
} while (0)

// Hand-assemble a 58-byte header with caller-chosen body_size. We
// write the fields directly rather than use pack_header because we
// specifically want a body_size that doesn't match any payload — the
// framer must reject the announced size before it ever reaches the
// body bytes.
std::array<std::uint8_t, oigtl::runtime::kHeaderSize>
hostile_header(std::uint64_t body_size, std::uint16_t version = 1) {
    std::array<std::uint8_t, oigtl::runtime::kHeaderSize> hdr{};
    oigtl::runtime::byte_order::write_be_u16(hdr.data() + 0, version);
    // type_id (12 bytes) — "STATUS" then NUL-padding.
    std::memcpy(hdr.data() + 2, "STATUS", 6);
    // device_name (20 bytes) left as zeros.
    // timestamp (8 bytes) at offset 34 left as zero.
    oigtl::runtime::byte_order::write_be_u64(hdr.data() + 42, body_size);
    // CRC at offset 50 — any value; we expect rejection before CRC.
    return hdr;
}

void test_body_size_uint64_max_rejected() {
    std::fprintf(stderr, "test_body_size_uint64_max_rejected\n");
    auto framer = oigtl::transport::make_v3_framer(/*max_body_size=*/0);
    auto hdr = hostile_header(std::numeric_limits<std::uint64_t>::max());

    std::vector<std::uint8_t> buffer(hdr.begin(), hdr.end());
    // Pad the buffer with some bytes so we'd *certainly* trip the
    // wrapped short-frame check if the guard weren't there.
    buffer.insert(buffer.end(), 128, 0xAA);

    bool caught = false;
    try {
        (void)framer->try_parse(buffer);
    } catch (const oigtl::transport::FramingError&) {
        caught = true;
    }
    REQUIRE(caught);
}

void test_body_size_near_size_max_rejected() {
    std::fprintf(stderr, "test_body_size_near_size_max_rejected\n");
    auto framer = oigtl::transport::make_v3_framer(/*max_body_size=*/0);
    // SIZE_MAX - kHeaderSize + 1 is the smallest value that still
    // wraps when kHeaderSize is added.
    const std::uint64_t edge =
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())
        - static_cast<std::uint64_t>(oigtl::runtime::kHeaderSize) + 1;
    auto hdr = hostile_header(edge);

    std::vector<std::uint8_t> buffer(hdr.begin(), hdr.end());
    buffer.insert(buffer.end(), 128, 0xAA);

    bool caught = false;
    try {
        (void)framer->try_parse(buffer);
    } catch (const oigtl::transport::FramingError&) {
        caught = true;
    }
    REQUIRE(caught);
}

void test_cap_still_fires_first_when_set() {
    // With max_body_size set, an over-cap body_size is rejected with
    // the original cap message, not the new overflow message. The
    // cap check still runs first (it's the public-facing policy).
    std::fprintf(stderr, "test_cap_still_fires_first_when_set\n");
    auto framer = oigtl::transport::make_v3_framer(/*max_body_size=*/1024);
    auto hdr = hostile_header(std::numeric_limits<std::uint64_t>::max());

    std::vector<std::uint8_t> buffer(hdr.begin(), hdr.end());
    buffer.insert(buffer.end(), 128, 0xAA);

    bool caught = false;
    std::string what;
    try {
        (void)framer->try_parse(buffer);
    } catch (const oigtl::transport::FramingError& e) {
        caught = true;
        what = e.what();
    }
    REQUIRE(caught);
    REQUIRE(what.find("max_message_size") != std::string::npos);
}

}  // namespace

int main() {
    test_body_size_uint64_max_rejected();
    test_body_size_near_size_max_rejected();
    test_cap_still_fires_first_when_set();

    if (g_fail_count == 0) {
        std::fprintf(stderr, "framer_overflow_test: all passed\n");
        return 0;
    }
    std::fprintf(stderr, "framer_overflow_test: %d failure(s)\n",
                 g_fail_count);
    return 1;
}
