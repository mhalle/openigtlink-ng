// GENERATED from spec/schemas/qtdata.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/qtdata.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Qtdata::pack() const {
    const std::size_t body_size = (tools.size() * 50);
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // tools
    for (std::size_t i = 0; i < tools.size(); ++i) {
        const auto& elem = tools[i];
        {
            constexpr std::size_t n = 20;
            std::size_t copy_len = elem.name.size() < n ? elem.name.size() : n;
            std::memcpy(out.data() + off, elem.name.data(), copy_len);
            if (copy_len < n) {
                std::memset(out.data() + off + copy_len, 0, n - copy_len);
            }
            off += 20;
        }
        oigtl::runtime::byte_order::write_be_u8(out.data() + off, elem.type);
        off += 1;
        oigtl::runtime::byte_order::write_be_u8(out.data() + off, elem.reserved);
        off += 1;
        for (std::size_t i = 0; i < 3; ++i) {
            oigtl::runtime::byte_order::write_be_f32(out.data() + off + i * 4, elem.position[i]);
        }
        off += 12;
        for (std::size_t i = 0; i < 4; ++i) {
            oigtl::runtime::byte_order::write_be_f32(out.data() + off + i * 4, elem.quaternion[i]);
        }
        off += 16;
    }
    // (off advanced inside loop)
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Qtdata Qtdata::unpack(const std::uint8_t* data, std::size_t length) {
    Qtdata out;
    std::size_t off = 0;
    // tools
    {
        std::size_t bytes = length - off;
        if (bytes % 50 != 0) { throw oigtl::error::MalformedMessageError("tools: trailing bytes not divisible by element size"); }
        std::size_t count = bytes / 50;
        out.tools.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            auto& elem = out.tools[i];
            {
                constexpr std::size_t n = 20;
                std::size_t len = 0;
                while (len < n && data[off + len] != 0) { ++len; }
                elem.name.assign(reinterpret_cast<const char*>(data + off), len);
                off += 20;
            }
            elem.type = oigtl::runtime::byte_order::read_be_u8(data + off);
            off += 1;
            elem.reserved = oigtl::runtime::byte_order::read_be_u8(data + off);
            off += 1;
            for (std::size_t i = 0; i < 3; ++i) {
                elem.position[i] = oigtl::runtime::byte_order::read_be_f32(data + off + i * 4);
            }
            off += 12;
            for (std::size_t i = 0; i < 4; ++i) {
                elem.quaternion[i] = oigtl::runtime::byte_order::read_be_f32(data + off + i * 4);
            }
            off += 16;
        }
    }
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
