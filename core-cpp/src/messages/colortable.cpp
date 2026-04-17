// GENERATED from spec/schemas/colortable.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/colortable.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/ascii.hpp"
#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Colortable::pack() const {
    const std::size_t body_size = (1) + (1) + (table.size() * 1);
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // index_type
    oigtl::runtime::byte_order::write_be_i8(out.data() + off, index_type);
    off += 1;
    // map_type
    oigtl::runtime::byte_order::write_be_i8(out.data() + off, map_type);
    off += 1;
    // table
    for (std::size_t i = 0; i < table.size(); ++i) {
        oigtl::runtime::byte_order::write_be_u8(out.data() + off + i * 1, table[i]);
    }
    off += table.size() * 1;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Colortable Colortable::unpack(const std::uint8_t* data, std::size_t length) {
    Colortable out;
    std::size_t off = 0;
    // index_type
    if (off + (1) > length) { throw oigtl::error::ShortBufferError("index_type: short buffer"); }
    out.index_type = oigtl::runtime::byte_order::read_be_i8(data + off);
    off += 1;
    // map_type
    if (off + (1) > length) { throw oigtl::error::ShortBufferError("map_type: short buffer"); }
    out.map_type = oigtl::runtime::byte_order::read_be_i8(data + off);
    off += 1;
    // table
    {
        std::size_t bytes = length - off;
        if (bytes % 1 != 0) { throw oigtl::error::MalformedMessageError("table: trailing bytes not divisible by element size"); }
        std::size_t count = bytes / 1;
        out.table.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            out.table[i] = oigtl::runtime::byte_order::read_be_u8(data + off + i * 1);
        }
        off += bytes;
    }
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
