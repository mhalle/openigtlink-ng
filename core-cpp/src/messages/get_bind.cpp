// GENERATED from spec/schemas/get_bind.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/get_bind.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/ascii.hpp"
#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> GetBind::pack() const {
    const std::size_t body_size = (2) + (type_ids.size() * 12) + (2) + (name_table.size() * 1);
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // ncmessages
    oigtl::runtime::byte_order::write_be_u16(out.data() + off, ncmessages);
    off += 2;
    // type_ids
    for (std::size_t i = 0; i < type_ids.size(); ++i) {
        {
            constexpr std::size_t n = 12;
            std::size_t copy_len = type_ids[i].size() < n ? type_ids[i].size() : n;
            std::memcpy(out.data() + off, type_ids[i].data(), copy_len);
            if (copy_len < n) {
                std::memset(out.data() + off + copy_len, 0, n - copy_len);
            }
            off += 12;
        }
    }
    // nametable_size
    oigtl::runtime::byte_order::write_be_u16(out.data() + off, nametable_size);
    off += 2;
    // name_table
    for (std::size_t i = 0; i < name_table.size(); ++i) {
        oigtl::runtime::byte_order::write_be_u8(out.data() + off + i * 1, name_table[i]);
    }
    off += name_table.size() * 1;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

GetBind GetBind::unpack(const std::uint8_t* data, std::size_t length) {
    GetBind out;
    std::size_t off = 0;
    // ncmessages
    if (off + (2) > length) { throw oigtl::error::ShortBufferError("ncmessages: short buffer"); }
    out.ncmessages = oigtl::runtime::byte_order::read_be_u16(data + off);
    off += 2;
    // type_ids
    {
        std::size_t count = static_cast<std::size_t>(out.ncmessages);
        if (off + (count * 12) > length) { throw oigtl::error::ShortBufferError("type_ids: short buffer"); }
        out.type_ids.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            {
            constexpr std::size_t n = 12;
            std::size_t len = 0;
            while (len < n && data[off + len] != 0) { ++len; }
            out.type_ids[i].assign(reinterpret_cast<const char*>(data + off), len);
            off += 12;
            }
        }
    }
    // nametable_size
    if (off + (2) > length) { throw oigtl::error::ShortBufferError("nametable_size: short buffer"); }
    out.nametable_size = oigtl::runtime::byte_order::read_be_u16(data + off);
    off += 2;
    // name_table
    {
        std::size_t count = static_cast<std::size_t>(out.nametable_size);
        if (off + (count * 1) > length) { throw oigtl::error::ShortBufferError("name_table: short buffer"); }
        out.name_table.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            out.name_table[i] = oigtl::runtime::byte_order::read_be_u8(data + off + i * 1);
        }
        off += count * 1;
    }
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
