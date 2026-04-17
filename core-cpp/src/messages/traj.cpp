// GENERATED from spec/schemas/traj.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/traj.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/ascii.hpp"
#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Traj::pack() const {
    const std::size_t body_size = (trajectories.size() * 150);
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // trajectories
    for (std::size_t i = 0; i < trajectories.size(); ++i) {
        const auto& elem = trajectories[i];
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
            constexpr std::size_t n = 32;
            std::size_t copy_len = elem.group_name.size() < n ? elem.group_name.size() : n;
            std::memcpy(out.data() + off, elem.group_name.data(), copy_len);
            if (copy_len < n) {
                std::memset(out.data() + off + copy_len, 0, n - copy_len);
            }
            off += 32;
        }
        oigtl::runtime::byte_order::write_be_i8(out.data() + off, elem.type);
        off += 1;
        oigtl::runtime::byte_order::write_be_i8(out.data() + off, elem.reserved);
        off += 1;
        for (std::size_t i = 0; i < 4; ++i) {
            oigtl::runtime::byte_order::write_be_u8(out.data() + off + i * 1, elem.rgba[i]);
        }
        off += 4;
        for (std::size_t i = 0; i < 3; ++i) {
            oigtl::runtime::byte_order::write_be_f32(out.data() + off + i * 4, elem.entry_pos[i]);
        }
        off += 12;
        for (std::size_t i = 0; i < 3; ++i) {
            oigtl::runtime::byte_order::write_be_f32(out.data() + off + i * 4, elem.target_pos[i]);
        }
        off += 12;
        oigtl::runtime::byte_order::write_be_f32(out.data() + off, elem.radius);
        off += 4;
        {
            constexpr std::size_t n = 20;
            std::size_t copy_len = elem.owner_name.size() < n ? elem.owner_name.size() : n;
            std::memcpy(out.data() + off, elem.owner_name.data(), copy_len);
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

Traj Traj::unpack(const std::uint8_t* data, std::size_t length) {
    Traj out;
    std::size_t off = 0;
    // trajectories
    {
        std::size_t bytes = length - off;
        if (bytes % 150 != 0) { throw oigtl::error::MalformedMessageError("trajectories: trailing bytes not divisible by element size"); }
        std::size_t count = bytes / 150;
        out.trajectories.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            auto& elem = out.trajectories[i];
            {
                constexpr std::size_t n = 64;
                const std::size_t len = oigtl::runtime::ascii::null_padded_length(data + off, n, "name");
                elem.name.assign(reinterpret_cast<const char*>(data + off), len);
                off += 64;
            }
            {
                constexpr std::size_t n = 32;
                const std::size_t len = oigtl::runtime::ascii::null_padded_length(data + off, n, "group_name");
                elem.group_name.assign(reinterpret_cast<const char*>(data + off), len);
                off += 32;
            }
            elem.type = oigtl::runtime::byte_order::read_be_i8(data + off);
            off += 1;
            elem.reserved = oigtl::runtime::byte_order::read_be_i8(data + off);
            off += 1;
            for (std::size_t i = 0; i < 4; ++i) {
                elem.rgba[i] = oigtl::runtime::byte_order::read_be_u8(data + off + i * 1);
            }
            off += 4;
            for (std::size_t i = 0; i < 3; ++i) {
                elem.entry_pos[i] = oigtl::runtime::byte_order::read_be_f32(data + off + i * 4);
            }
            off += 12;
            for (std::size_t i = 0; i < 3; ++i) {
                elem.target_pos[i] = oigtl::runtime::byte_order::read_be_f32(data + off + i * 4);
            }
            off += 12;
            elem.radius = oigtl::runtime::byte_order::read_be_f32(data + off);
            off += 4;
            {
                constexpr std::size_t n = 20;
                const std::size_t len = oigtl::runtime::ascii::null_padded_length(data + off, n, "owner_name");
                elem.owner_name.assign(reinterpret_cast<const char*>(data + off), len);
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
