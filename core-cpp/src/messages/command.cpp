// GENERATED from spec/schemas/command.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/command.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Command::pack() const {
    const std::size_t body_size = (4) + (128) + (2) + (4) + (command.size() * 1);
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // command_id
    oigtl::runtime::byte_order::write_be_u32(out.data() + off, command_id);
    off += 4;
    // command_name
    {
        constexpr std::size_t n = 128;
        std::size_t copy_len = command_name.size() < n ? command_name.size() : n;
        std::memcpy(out.data() + off, command_name.data(), copy_len);
        if (copy_len < n) {
            std::memset(out.data() + off + copy_len, 0, n - copy_len);
        }
        off += 128;
    }
    // encoding
    oigtl::runtime::byte_order::write_be_u16(out.data() + off, encoding);
    off += 2;
    // length
    oigtl::runtime::byte_order::write_be_u32(out.data() + off, length);
    off += 4;
    // command
    for (std::size_t i = 0; i < command.size(); ++i) {
        oigtl::runtime::byte_order::write_be_u8(out.data() + off + i * 1, command[i]);
    }
    off += command.size() * 1;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Command Command::unpack(const std::uint8_t* data, std::size_t length) {
    Command out;
    std::size_t off = 0;
    // command_id
    if (off + (4) > length) { throw oigtl::error::ShortBufferError("command_id: short buffer"); }
    out.command_id = oigtl::runtime::byte_order::read_be_u32(data + off);
    off += 4;
    // command_name
    if (off + (128) > length) { throw oigtl::error::ShortBufferError("command_name: short buffer"); }
    {
        constexpr std::size_t n = 128;
        std::size_t len = 0;
        while (len < n && data[off + len] != 0) { ++len; }
        out.command_name.assign(reinterpret_cast<const char*>(data + off), len);
        off += 128;
    }
    // encoding
    if (off + (2) > length) { throw oigtl::error::ShortBufferError("encoding: short buffer"); }
    out.encoding = oigtl::runtime::byte_order::read_be_u16(data + off);
    off += 2;
    // length
    if (off + (4) > length) { throw oigtl::error::ShortBufferError("length: short buffer"); }
    out.length = oigtl::runtime::byte_order::read_be_u32(data + off);
    off += 4;
    // command
    {
        std::size_t count = static_cast<std::size_t>(out.length);
        if (off + (count * 1) > length) { throw oigtl::error::ShortBufferError("command: short buffer"); }
        out.command.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            out.command[i] = oigtl::runtime::byte_order::read_be_u8(data + off + i * 1);
        }
        off += count * 1;
    }
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
