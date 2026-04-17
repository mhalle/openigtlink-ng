// GENERATED from spec/schemas/metadata.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/metadata.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Metadata::pack() const {
    const std::size_t body_size = (2) + (index_entries.size() * 8) + (body.size() * 1);
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // count
    oigtl::runtime::byte_order::write_be_u16(out.data() + off, count);
    off += 2;
    // index_entries
    for (std::size_t i = 0; i < index_entries.size(); ++i) {
        const auto& elem = index_entries[i];
        oigtl::runtime::byte_order::write_be_u16(out.data() + off, elem.key_size);
        off += 2;
        oigtl::runtime::byte_order::write_be_u16(out.data() + off, elem.value_encoding);
        off += 2;
        oigtl::runtime::byte_order::write_be_u32(out.data() + off, elem.value_size);
        off += 4;
    }
    // (off advanced inside loop)
    // body
    for (std::size_t i = 0; i < body.size(); ++i) {
        oigtl::runtime::byte_order::write_be_u8(out.data() + off + i * 1, body[i]);
    }
    off += body.size() * 1;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Metadata Metadata::unpack(const std::uint8_t* data, std::size_t length) {
    Metadata out;
    std::size_t off = 0;
    // count
    if (off + (2) > length) { throw oigtl::error::ShortBufferError("count: short buffer"); }
    out.count = oigtl::runtime::byte_order::read_be_u16(data + off);
    off += 2;
    // index_entries
    {
        std::size_t count = static_cast<std::size_t>(out.count);
        if (off + (count * 8) > length) { throw oigtl::error::ShortBufferError("index_entries: short buffer"); }
        out.index_entries.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            auto& elem = out.index_entries[i];
            elem.key_size = oigtl::runtime::byte_order::read_be_u16(data + off);
            off += 2;
            elem.value_encoding = oigtl::runtime::byte_order::read_be_u16(data + off);
            off += 2;
            elem.value_size = oigtl::runtime::byte_order::read_be_u32(data + off);
            off += 4;
        }
    }
    // body
    {
        std::size_t bytes = length - off;
        if (bytes % 1 != 0) { throw oigtl::error::MalformedMessageError("body: trailing bytes not divisible by element size"); }
        std::size_t count = bytes / 1;
        out.body.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            out.body[i] = oigtl::runtime::byte_order::read_be_u8(data + off + i * 1);
        }
        off += bytes;
    }
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
