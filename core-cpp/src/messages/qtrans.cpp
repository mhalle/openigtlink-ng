// GENERATED from spec/schemas/qtrans.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/qtrans.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/ascii.hpp"
#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Qtrans::pack() const {
    const std::size_t body_size = 28;
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // position
    for (std::size_t i = 0; i < 3; ++i) {
        oigtl::runtime::byte_order::write_be_f32(out.data() + off + i * 4, position[i]);
    }
    off += 12;
    // quaternion
    for (std::size_t i = 0; i < 4; ++i) {
        oigtl::runtime::byte_order::write_be_f32(out.data() + off + i * 4, quaternion[i]);
    }
    off += 16;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Qtrans Qtrans::unpack(const std::uint8_t* data, std::size_t length) {
    if (length < kBodySize) {
        std::ostringstream oss;
        oss << "QTRANS body requires " << kBodySize
            << " bytes, got " << length;
        throw oigtl::error::ShortBufferError(oss.str());
    }
    Qtrans out;
    std::size_t off = 0;
    // position
    if (off + (12) > length) { throw oigtl::error::ShortBufferError("position: short buffer"); }
    for (std::size_t i = 0; i < 3; ++i) {
        out.position[i] = oigtl::runtime::byte_order::read_be_f32(data + off + i * 4);
    }
    off += 12;
    // quaternion
    if (off + (16) > length) { throw oigtl::error::ShortBufferError("quaternion: short buffer"); }
    for (std::size_t i = 0; i < 4; ++i) {
        out.quaternion[i] = oigtl::runtime::byte_order::read_be_f32(data + off + i * 4);
    }
    off += 16;
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
