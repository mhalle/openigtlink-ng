// GENERATED from spec/schemas/image.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/image.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/ascii.hpp"
#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"
#include "oigtl/runtime/invariants.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Image::pack() const {
    const std::size_t body_size = (2) + (1) + (1) + (1) + (1) + (6) + (48) + (6) + (6) + (pixels.size() * 1);
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // header_version
    oigtl::runtime::byte_order::write_be_u16(out.data() + off, header_version);
    off += 2;
    // num_components
    oigtl::runtime::byte_order::write_be_u8(out.data() + off, num_components);
    off += 1;
    // scalar_type
    oigtl::runtime::byte_order::write_be_u8(out.data() + off, scalar_type);
    off += 1;
    // endian
    oigtl::runtime::byte_order::write_be_u8(out.data() + off, endian);
    off += 1;
    // coord
    oigtl::runtime::byte_order::write_be_u8(out.data() + off, coord);
    off += 1;
    // size
    for (std::size_t i = 0; i < 3; ++i) {
        oigtl::runtime::byte_order::write_be_u16(out.data() + off + i * 2, size[i]);
    }
    off += 6;
    // matrix
    for (std::size_t i = 0; i < 12; ++i) {
        oigtl::runtime::byte_order::write_be_f32(out.data() + off + i * 4, matrix[i]);
    }
    off += 48;
    // subvol_offset
    for (std::size_t i = 0; i < 3; ++i) {
        oigtl::runtime::byte_order::write_be_u16(out.data() + off + i * 2, subvol_offset[i]);
    }
    off += 6;
    // subvol_size
    for (std::size_t i = 0; i < 3; ++i) {
        oigtl::runtime::byte_order::write_be_u16(out.data() + off + i * 2, subvol_size[i]);
    }
    off += 6;
    // pixels
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        oigtl::runtime::byte_order::write_be_u8(out.data() + off + i * 1, pixels[i]);
    }
    off += pixels.size() * 1;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Image Image::unpack(const std::uint8_t* data, std::size_t length) {
    Image out;
    std::size_t off = 0;
    // header_version
    if (off + (2) > length) { throw oigtl::error::ShortBufferError("header_version: short buffer"); }
    out.header_version = oigtl::runtime::byte_order::read_be_u16(data + off);
    off += 2;
    // num_components
    if (off + (1) > length) { throw oigtl::error::ShortBufferError("num_components: short buffer"); }
    out.num_components = oigtl::runtime::byte_order::read_be_u8(data + off);
    off += 1;
    // scalar_type
    if (off + (1) > length) { throw oigtl::error::ShortBufferError("scalar_type: short buffer"); }
    out.scalar_type = oigtl::runtime::byte_order::read_be_u8(data + off);
    off += 1;
    // endian
    if (off + (1) > length) { throw oigtl::error::ShortBufferError("endian: short buffer"); }
    out.endian = oigtl::runtime::byte_order::read_be_u8(data + off);
    off += 1;
    // coord
    if (off + (1) > length) { throw oigtl::error::ShortBufferError("coord: short buffer"); }
    out.coord = oigtl::runtime::byte_order::read_be_u8(data + off);
    off += 1;
    // size
    if (off + (6) > length) { throw oigtl::error::ShortBufferError("size: short buffer"); }
    for (std::size_t i = 0; i < 3; ++i) {
        out.size[i] = oigtl::runtime::byte_order::read_be_u16(data + off + i * 2);
    }
    off += 6;
    // matrix
    if (off + (48) > length) { throw oigtl::error::ShortBufferError("matrix: short buffer"); }
    for (std::size_t i = 0; i < 12; ++i) {
        out.matrix[i] = oigtl::runtime::byte_order::read_be_f32(data + off + i * 4);
    }
    off += 48;
    // subvol_offset
    if (off + (6) > length) { throw oigtl::error::ShortBufferError("subvol_offset: short buffer"); }
    for (std::size_t i = 0; i < 3; ++i) {
        out.subvol_offset[i] = oigtl::runtime::byte_order::read_be_u16(data + off + i * 2);
    }
    off += 6;
    // subvol_size
    if (off + (6) > length) { throw oigtl::error::ShortBufferError("subvol_size: short buffer"); }
    for (std::size_t i = 0; i < 3; ++i) {
        out.subvol_size[i] = oigtl::runtime::byte_order::read_be_u16(data + off + i * 2);
    }
    off += 6;
    // pixels
    {
        std::size_t bytes = length - off;
        if (bytes % 1 != 0) { throw oigtl::error::MalformedMessageError("pixels: trailing bytes not divisible by element size"); }
        std::size_t count = bytes / 1;
        out.pixels.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            out.pixels[i] = oigtl::runtime::byte_order::read_be_u8(data + off + i * 1);
        }
        off += bytes;
    }
    (void)off;
    (void)data;
    (void)length;
    // Post-unpack cross-field invariant (schema:
    //   post_unpack_invariant = "image").
    // Parallel to python_message.py.jinja + ts_message.ts.jinja +
    // corpus-tools codec/policy.py::POST_UNPACK_INVARIANTS.
    oigtl::runtime::invariants::check_image(out);
    return out;
}

}  // namespace oigtl::messages
