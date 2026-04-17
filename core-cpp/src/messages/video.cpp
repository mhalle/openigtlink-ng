// GENERATED from spec/schemas/video.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/video.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Video::pack() const {
    const std::size_t body_size = (2) + (1) + (4) + (2) + (1) + (6) + (48) + (6) + (6) + (frame_data.size() * 1);
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // header_version
    oigtl::runtime::byte_order::write_be_u16(out.data() + off, header_version);
    off += 2;
    // endian
    oigtl::runtime::byte_order::write_be_u8(out.data() + off, endian);
    off += 1;
    // codec
    {
        constexpr std::size_t n = 4;
        std::size_t copy_len = codec.size() < n ? codec.size() : n;
        std::memcpy(out.data() + off, codec.data(), copy_len);
        if (copy_len < n) {
            std::memset(out.data() + off + copy_len, 0, n - copy_len);
        }
        off += 4;
    }
    // frame_type
    oigtl::runtime::byte_order::write_be_u16(out.data() + off, frame_type);
    off += 2;
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
    // frame_data
    for (std::size_t i = 0; i < frame_data.size(); ++i) {
        oigtl::runtime::byte_order::write_be_u8(out.data() + off + i * 1, frame_data[i]);
    }
    off += frame_data.size() * 1;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Video Video::unpack(const std::uint8_t* data, std::size_t length) {
    Video out;
    std::size_t off = 0;
    // header_version
    if (off + (2) > length) { throw oigtl::error::ShortBufferError("header_version: short buffer"); }
    out.header_version = oigtl::runtime::byte_order::read_be_u16(data + off);
    off += 2;
    // endian
    if (off + (1) > length) { throw oigtl::error::ShortBufferError("endian: short buffer"); }
    out.endian = oigtl::runtime::byte_order::read_be_u8(data + off);
    off += 1;
    // codec
    if (off + (4) > length) { throw oigtl::error::ShortBufferError("codec: short buffer"); }
    out.codec.assign(reinterpret_cast<const char*>(data + off), 4);
    off += 4;
    // frame_type
    if (off + (2) > length) { throw oigtl::error::ShortBufferError("frame_type: short buffer"); }
    out.frame_type = oigtl::runtime::byte_order::read_be_u16(data + off);
    off += 2;
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
    // frame_data
    {
        std::size_t bytes = length - off;
        if (bytes % 1 != 0) { throw oigtl::error::MalformedMessageError("frame_data: trailing bytes not divisible by element size"); }
        std::size_t count = bytes / 1;
        out.frame_data.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            out.frame_data[i] = oigtl::runtime::byte_order::read_be_u8(data + off + i * 1);
        }
        off += bytes;
    }
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
