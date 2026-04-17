// GENERATED from spec/schemas/bind.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/bind.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/ascii.hpp"
#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"
#include "oigtl/runtime/invariants.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Bind::pack() const {
    const std::size_t body_size = (2) + (header_entries.size() * 20) + (2) + (name_table.size() * 1) + (bodies.size() * 1);
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // ncmessages
    oigtl::runtime::byte_order::write_be_u16(out.data() + off, ncmessages);
    off += 2;
    // header_entries
    for (std::size_t i = 0; i < header_entries.size(); ++i) {
        const auto& elem = header_entries[i];
        {
            constexpr std::size_t n = 12;
            std::size_t copy_len = elem.type_id.size() < n ? elem.type_id.size() : n;
            std::memcpy(out.data() + off, elem.type_id.data(), copy_len);
            if (copy_len < n) {
                std::memset(out.data() + off + copy_len, 0, n - copy_len);
            }
            off += 12;
        }
        oigtl::runtime::byte_order::write_be_u64(out.data() + off, elem.body_size);
        off += 8;
    }
    // (off advanced inside loop)
    // nametable_size
    oigtl::runtime::byte_order::write_be_u16(out.data() + off, nametable_size);
    off += 2;
    // name_table
    for (std::size_t i = 0; i < name_table.size(); ++i) {
        oigtl::runtime::byte_order::write_be_u8(out.data() + off + i * 1, name_table[i]);
    }
    off += name_table.size() * 1;
    // bodies
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        oigtl::runtime::byte_order::write_be_u8(out.data() + off + i * 1, bodies[i]);
    }
    off += bodies.size() * 1;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Bind Bind::unpack(const std::uint8_t* data, std::size_t length) {
    Bind out;
    std::size_t off = 0;
    // ncmessages
    if (off + (2) > length) { throw oigtl::error::ShortBufferError("ncmessages: short buffer"); }
    out.ncmessages = oigtl::runtime::byte_order::read_be_u16(data + off);
    off += 2;
    // header_entries
    {
        std::size_t count = static_cast<std::size_t>(out.ncmessages);
        if (off + (count * 20) > length) { throw oigtl::error::ShortBufferError("header_entries: short buffer"); }
        out.header_entries.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            auto& elem = out.header_entries[i];
            {
                constexpr std::size_t n = 12;
                const std::size_t len = oigtl::runtime::ascii::null_padded_length(data + off, n, "type_id");
                elem.type_id.assign(reinterpret_cast<const char*>(data + off), len);
                off += 12;
            }
            elem.body_size = oigtl::runtime::byte_order::read_be_u64(data + off);
            off += 8;
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
    // bodies
    {
        std::size_t bytes = length - off;
        if (bytes % 1 != 0) { throw oigtl::error::MalformedMessageError("bodies: trailing bytes not divisible by element size"); }
        std::size_t count = bytes / 1;
        out.bodies.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            out.bodies[i] = oigtl::runtime::byte_order::read_be_u8(data + off + i * 1);
        }
        off += bytes;
    }
    (void)off;
    (void)data;
    (void)length;
    // Post-unpack cross-field invariant (schema:
    //   post_unpack_invariant = "bind").
    // Parallel to python_message.py.jinja + ts_message.ts.jinja +
    // corpus-tools codec/policy.py::POST_UNPACK_INVARIANTS.
    oigtl::runtime::invariants::check_bind(out);
    return out;
}

}  // namespace oigtl::messages
