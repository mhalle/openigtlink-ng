// Regression test: compat MessageBase must reject a hostile
// body_size that would overflow size_t when added to the 58-byte
// header size. Two paths to exercise:
//
//   - Unpack(): the wire buffer carries a header whose body_size is
//     near UINT64_MAX. Before the fix, `kHeaderSize + body_size`
//     wrapped on size_t and the "body fully present" check passed
//     incorrectly, feeding out-of-range pointer arithmetic into
//     UnpackContent. After the fix, Unpack returns the UNPACK_HEADER
//     status only (the body is "not yet available").
//
//   - AllocateBuffer(): m_BodySizeToRead is peer-supplied and may
//     contain a hostile value. Before the fix, vector::resize got a
//     wrapped (small) size and silently shrunk the buffer, setting
//     up a heap overrun on the subsequent body recv. After the fix,
//     std::length_error is thrown.
//
// We exercise both via a small subclass that exposes the protected
// m_Wire / m_BodySizeToRead so the test can plant the hostile value
// without going through a real network.

#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

#include "igtl/igtlMessageBase.h"
#include "igtl/igtlTransformMessage.h"

#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/header.hpp"

namespace {

int g_fail_count = 0;

#define REQUIRE(expr) do {                                            \
    if (!(expr)) {                                                    \
        std::fprintf(stderr, "  FAIL: %s:%d  %s\n",                   \
                     __FILE__, __LINE__, #expr);                      \
        ++g_fail_count;                                               \
    }                                                                 \
} while (0)

// Probe: exposes the protected wire + body-size-to-read so the test
// can plant a hostile header without involving a real socket.
class TransformOverflowProbe : public igtl::TransformMessage {
 public:
    using igtl::TransformMessage::m_Wire;
    using igtl::TransformMessage::m_BodySizeToRead;
};

// Fill wire with a well-formed 58-byte header announcing `body_size`.
// No body bytes follow; that's intentional — we want to trip the
// guard, not exercise body parsing.
void plant_hostile_header(std::vector<std::uint8_t>& wire,
                          std::uint64_t body_size) {
    wire.assign(oigtl::runtime::kHeaderSize, 0);
    oigtl::runtime::byte_order::write_be_u16(wire.data() + 0, 1);
    std::memcpy(wire.data() + 2, "TRANSFORM", 9);
    oigtl::runtime::byte_order::write_be_u64(wire.data() + 42, body_size);
}

void test_unpack_with_overflow_body_size_stops_at_header() {
    std::fprintf(stderr,
                 "test_unpack_with_overflow_body_size_stops_at_header\n");
    TransformOverflowProbe msg;
    plant_hostile_header(msg.m_Wire,
                         std::numeric_limits<std::uint64_t>::max());

    // crccheck=false — we want the guard to fire before CRC runs, and
    // we've planted an all-zero CRC field anyway.
    const int status = msg.Unpack(0);
    // Header should parse; body should not.
    REQUIRE((status & igtl::UNPACK_HEADER) != 0);
    REQUIRE((status & igtl::UNPACK_BODY) == 0);
}

void test_allocate_buffer_throws_on_overflow_body_size() {
    std::fprintf(stderr,
                 "test_allocate_buffer_throws_on_overflow_body_size\n");
    TransformOverflowProbe msg;
    msg.m_BodySizeToRead = std::numeric_limits<std::uint64_t>::max();

    bool caught = false;
    try {
        msg.AllocateBuffer();
    } catch (const std::length_error&) {
        caught = true;
    }
    REQUIRE(caught);
}

}  // namespace

int main() {
    test_unpack_with_overflow_body_size_stops_at_header();
    test_allocate_buffer_throws_on_overflow_body_size();

    if (g_fail_count == 0) {
        std::fprintf(stderr, "messagebase_overflow_test: all passed\n");
        return 0;
    }
    std::fprintf(stderr, "messagebase_overflow_test: %d failure(s)\n",
                 g_fail_count);
    return 1;
}
