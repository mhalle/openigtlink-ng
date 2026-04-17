// GENERATED from spec/schemas/position.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/position.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/ascii.hpp"
#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Position::pack() const {
    const std::size_t body_size = (12) + (quaternion.size() * 4);
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // position
    for (std::size_t i = 0; i < 3; ++i) {
        oigtl::runtime::byte_order::write_be_f32(out.data() + off + i * 4, position[i]);
    }
    off += 12;
    // quaternion
    for (std::size_t i = 0; i < quaternion.size(); ++i) {
        oigtl::runtime::byte_order::write_be_f32(out.data() + off + i * 4, quaternion[i]);
    }
    off += quaternion.size() * 4;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Position Position::unpack(const std::uint8_t* data, std::size_t length) {
    {
        const std::size_t _allowed[] = { 12, 24, 28 };
        bool _ok = false;
        for (std::size_t _v : _allowed) { if (length == _v) { _ok = true; break; } }
        if (!_ok) {
            std::ostringstream oss;
            oss << "POSITION body_size=" << length
                << " is not in the allowed set { 12, 24, 28 }";
            throw oigtl::error::MalformedMessageError(oss.str());
        }
    }
    Position out;
    std::size_t off = 0;
    // position
    if (off + (12) > length) { throw oigtl::error::ShortBufferError("position: short buffer"); }
    for (std::size_t i = 0; i < 3; ++i) {
        out.position[i] = oigtl::runtime::byte_order::read_be_f32(data + off + i * 4);
    }
    off += 12;
    // quaternion
    {
        std::size_t bytes = length - off;
        if (bytes % 4 != 0) { throw oigtl::error::MalformedMessageError("quaternion: trailing bytes not divisible by element size"); }
        std::size_t count = bytes / 4;
        out.quaternion.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            out.quaternion[i] = oigtl::runtime::byte_order::read_be_f32(data + off + i * 4);
        }
        off += bytes;
    }
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
