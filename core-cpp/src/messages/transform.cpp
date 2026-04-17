// GENERATED from spec/schemas/transform.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/transform.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/ascii.hpp"
#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Transform::pack() const {
    const std::size_t body_size = 48;
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // matrix
    for (std::size_t i = 0; i < 12; ++i) {
        oigtl::runtime::byte_order::write_be_f32(out.data() + off + i * 4, matrix[i]);
    }
    off += 48;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Transform Transform::unpack(const std::uint8_t* data, std::size_t length) {
    if (length < kBodySize) {
        std::ostringstream oss;
        oss << "TRANSFORM body requires " << kBodySize
            << " bytes, got " << length;
        throw oigtl::error::ShortBufferError(oss.str());
    }
    Transform out;
    std::size_t off = 0;
    // matrix
    if (off + (48) > length) { throw oigtl::error::ShortBufferError("matrix: short buffer"); }
    for (std::size_t i = 0; i < 12; ++i) {
        out.matrix[i] = oigtl::runtime::byte_order::read_be_f32(data + off + i * 4);
    }
    off += 48;
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
