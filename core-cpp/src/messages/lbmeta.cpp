// GENERATED from spec/schemas/lbmeta.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/lbmeta.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/ascii.hpp"
#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Lbmeta::pack() const {
    const std::size_t body_size = (labels.size() * 116);
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // labels
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const auto& elem = labels[i];
        {
            constexpr std::size_t n = 64;
            std::size_t copy_len = elem.name.size() < n ? elem.name.size() : n;
            std::memcpy(out.data() + off, elem.name.data(), copy_len);
            if (copy_len < n) {
                std::memset(out.data() + off + copy_len, 0, n - copy_len);
            }
            off += 64;
        }
        {
            constexpr std::size_t n = 20;
            std::size_t copy_len = elem.device_name.size() < n ? elem.device_name.size() : n;
            std::memcpy(out.data() + off, elem.device_name.data(), copy_len);
            if (copy_len < n) {
                std::memset(out.data() + off + copy_len, 0, n - copy_len);
            }
            off += 20;
        }
        oigtl::runtime::byte_order::write_be_u8(out.data() + off, elem.label);
        off += 1;
        oigtl::runtime::byte_order::write_be_u8(out.data() + off, elem.reserved);
        off += 1;
        for (std::size_t i = 0; i < 4; ++i) {
            oigtl::runtime::byte_order::write_be_u8(out.data() + off + i * 1, elem.rgba[i]);
        }
        off += 4;
        for (std::size_t i = 0; i < 3; ++i) {
            oigtl::runtime::byte_order::write_be_u16(out.data() + off + i * 2, elem.size[i]);
        }
        off += 6;
        {
            constexpr std::size_t n = 20;
            std::size_t copy_len = elem.owner.size() < n ? elem.owner.size() : n;
            std::memcpy(out.data() + off, elem.owner.data(), copy_len);
            if (copy_len < n) {
                std::memset(out.data() + off + copy_len, 0, n - copy_len);
            }
            off += 20;
        }
    }
    // (off advanced inside loop)
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Lbmeta Lbmeta::unpack(const std::uint8_t* data, std::size_t length) {
    Lbmeta out;
    std::size_t off = 0;
    // labels
    {
        std::size_t bytes = length - off;
        if (bytes % 116 != 0) { throw oigtl::error::MalformedMessageError("labels: trailing bytes not divisible by element size"); }
        std::size_t count = bytes / 116;
        out.labels.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            auto& elem = out.labels[i];
            {
                constexpr std::size_t n = 64;
                const std::size_t len = oigtl::runtime::ascii::null_padded_length(data + off, n, "name");
                elem.name.assign(reinterpret_cast<const char*>(data + off), len);
                off += 64;
            }
            {
                constexpr std::size_t n = 20;
                const std::size_t len = oigtl::runtime::ascii::null_padded_length(data + off, n, "device_name");
                elem.device_name.assign(reinterpret_cast<const char*>(data + off), len);
                off += 20;
            }
            elem.label = oigtl::runtime::byte_order::read_be_u8(data + off);
            off += 1;
            elem.reserved = oigtl::runtime::byte_order::read_be_u8(data + off);
            off += 1;
            for (std::size_t i = 0; i < 4; ++i) {
                elem.rgba[i] = oigtl::runtime::byte_order::read_be_u8(data + off + i * 1);
            }
            off += 4;
            for (std::size_t i = 0; i < 3; ++i) {
                elem.size[i] = oigtl::runtime::byte_order::read_be_u16(data + off + i * 2);
            }
            off += 6;
            {
                constexpr std::size_t n = 20;
                const std::size_t len = oigtl::runtime::ascii::null_padded_length(data + off, n, "owner");
                elem.owner.assign(reinterpret_cast<const char*>(data + off), len);
                off += 20;
            }
        }
    }
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
