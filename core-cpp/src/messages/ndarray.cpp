// GENERATED from spec/schemas/ndarray.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/ndarray.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Ndarray::pack() const {
    const std::size_t body_size = (1) + (1) + (size.size() * 2) + (data.size() * 1);
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // scalar_type
    oigtl::runtime::byte_order::write_be_u8(out.data() + off, scalar_type);
    off += 1;
    // dim
    oigtl::runtime::byte_order::write_be_u8(out.data() + off, dim);
    off += 1;
    // size
    for (std::size_t i = 0; i < size.size(); ++i) {
        oigtl::runtime::byte_order::write_be_u16(out.data() + off + i * 2, size[i]);
    }
    off += size.size() * 2;
    // data
    for (std::size_t i = 0; i < data.size(); ++i) {
        oigtl::runtime::byte_order::write_be_u8(out.data() + off + i * 1, data[i]);
    }
    off += data.size() * 1;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Ndarray Ndarray::unpack(const std::uint8_t* data, std::size_t length) {
    Ndarray out;
    std::size_t off = 0;
    // scalar_type
    if (off + (1) > length) { throw oigtl::error::ShortBufferError("scalar_type: short buffer"); }
    out.scalar_type = oigtl::runtime::byte_order::read_be_u8(data + off);
    off += 1;
    // dim
    if (off + (1) > length) { throw oigtl::error::ShortBufferError("dim: short buffer"); }
    out.dim = oigtl::runtime::byte_order::read_be_u8(data + off);
    off += 1;
    // size
    {
        std::size_t count = static_cast<std::size_t>(out.dim);
        if (off + (count * 2) > length) { throw oigtl::error::ShortBufferError("size: short buffer"); }
        out.size.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            out.size[i] = oigtl::runtime::byte_order::read_be_u16(data + off + i * 2);
        }
        off += count * 2;
    }
    // data
    {
        std::size_t bytes = length - off;
        if (bytes % 1 != 0) { throw oigtl::error::MalformedMessageError("data: trailing bytes not divisible by element size"); }
        std::size_t count = bytes / 1;
        out.data.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            out.data[i] = oigtl::runtime::byte_order::read_be_u8(data + off + i * 1);
        }
        off += bytes;
    }
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
