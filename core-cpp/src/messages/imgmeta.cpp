// GENERATED from spec/schemas/imgmeta.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/imgmeta.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Imgmeta::pack() const {
    const std::size_t body_size = (images.size() * 260);
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // images
    for (std::size_t i = 0; i < images.size(); ++i) {
        const auto& elem = images[i];
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
        {
            constexpr std::size_t n = 32;
            std::size_t copy_len = elem.modality.size() < n ? elem.modality.size() : n;
            std::memcpy(out.data() + off, elem.modality.data(), copy_len);
            if (copy_len < n) {
                std::memset(out.data() + off + copy_len, 0, n - copy_len);
            }
            off += 32;
        }
        {
            constexpr std::size_t n = 64;
            std::size_t copy_len = elem.patient_name.size() < n ? elem.patient_name.size() : n;
            std::memcpy(out.data() + off, elem.patient_name.data(), copy_len);
            if (copy_len < n) {
                std::memset(out.data() + off + copy_len, 0, n - copy_len);
            }
            off += 64;
        }
        {
            constexpr std::size_t n = 64;
            std::size_t copy_len = elem.patient_id.size() < n ? elem.patient_id.size() : n;
            std::memcpy(out.data() + off, elem.patient_id.data(), copy_len);
            if (copy_len < n) {
                std::memset(out.data() + off + copy_len, 0, n - copy_len);
            }
            off += 64;
        }
        oigtl::runtime::byte_order::write_be_u64(out.data() + off, elem.timestamp);
        off += 8;
        for (std::size_t i = 0; i < 3; ++i) {
            oigtl::runtime::byte_order::write_be_u16(out.data() + off + i * 2, elem.size[i]);
        }
        off += 6;
        oigtl::runtime::byte_order::write_be_u8(out.data() + off, elem.scalar_type);
        off += 1;
        oigtl::runtime::byte_order::write_be_u8(out.data() + off, elem.reserved);
        off += 1;
    }
    // (off advanced inside loop)
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Imgmeta Imgmeta::unpack(const std::uint8_t* data, std::size_t length) {
    Imgmeta out;
    std::size_t off = 0;
    // images
    {
        std::size_t bytes = length - off;
        if (bytes % 260 != 0) { throw oigtl::error::MalformedMessageError("images: trailing bytes not divisible by element size"); }
        std::size_t count = bytes / 260;
        out.images.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            auto& elem = out.images[i];
            {
                constexpr std::size_t n = 64;
                std::size_t len = 0;
                while (len < n && data[off + len] != 0) { ++len; }
                elem.name.assign(reinterpret_cast<const char*>(data + off), len);
                off += 64;
            }
            {
                constexpr std::size_t n = 20;
                std::size_t len = 0;
                while (len < n && data[off + len] != 0) { ++len; }
                elem.device_name.assign(reinterpret_cast<const char*>(data + off), len);
                off += 20;
            }
            {
                constexpr std::size_t n = 32;
                std::size_t len = 0;
                while (len < n && data[off + len] != 0) { ++len; }
                elem.modality.assign(reinterpret_cast<const char*>(data + off), len);
                off += 32;
            }
            {
                constexpr std::size_t n = 64;
                std::size_t len = 0;
                while (len < n && data[off + len] != 0) { ++len; }
                elem.patient_name.assign(reinterpret_cast<const char*>(data + off), len);
                off += 64;
            }
            {
                constexpr std::size_t n = 64;
                std::size_t len = 0;
                while (len < n && data[off + len] != 0) { ++len; }
                elem.patient_id.assign(reinterpret_cast<const char*>(data + off), len);
                off += 64;
            }
            elem.timestamp = oigtl::runtime::byte_order::read_be_u64(data + off);
            off += 8;
            for (std::size_t i = 0; i < 3; ++i) {
                elem.size[i] = oigtl::runtime::byte_order::read_be_u16(data + off + i * 2);
            }
            off += 6;
            elem.scalar_type = oigtl::runtime::byte_order::read_be_u8(data + off);
            off += 1;
            elem.reserved = oigtl::runtime::byte_order::read_be_u8(data + off);
            off += 1;
        }
    }
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
