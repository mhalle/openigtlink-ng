// GENERATED from spec/schemas/header.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/header.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Header::pack() const {
    const std::size_t body_size = 58;
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // version
    oigtl::runtime::byte_order::write_be_u16(out.data() + off, version);
    off += 2;
    // type
    {
        constexpr std::size_t n = 12;
        std::size_t copy_len = type.size() < n ? type.size() : n;
        std::memcpy(out.data() + off, type.data(), copy_len);
        if (copy_len < n) {
            std::memset(out.data() + off + copy_len, 0, n - copy_len);
        }
        off += 12;
    }
    // device_name
    {
        constexpr std::size_t n = 20;
        std::size_t copy_len = device_name.size() < n ? device_name.size() : n;
        std::memcpy(out.data() + off, device_name.data(), copy_len);
        if (copy_len < n) {
            std::memset(out.data() + off + copy_len, 0, n - copy_len);
        }
        off += 20;
    }
    // timestamp
    oigtl::runtime::byte_order::write_be_u64(out.data() + off, timestamp);
    off += 8;
    // body_size
    oigtl::runtime::byte_order::write_be_u64(out.data() + off, body_size);
    off += 8;
    // crc
    oigtl::runtime::byte_order::write_be_u64(out.data() + off, crc);
    off += 8;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Header Header::unpack(const std::uint8_t* data, std::size_t length) {
    if (length < kBodySize) {
        std::ostringstream oss;
        oss << "HEADER body requires " << kBodySize
            << " bytes, got " << length;
        throw oigtl::error::ShortBufferError(oss.str());
    }
    Header out;
    std::size_t off = 0;
    // version
    if (off + (2) > length) { throw oigtl::error::ShortBufferError("version: short buffer"); }
    out.version = oigtl::runtime::byte_order::read_be_u16(data + off);
    off += 2;
    // type
    if (off + (12) > length) { throw oigtl::error::ShortBufferError("type: short buffer"); }
    {
        constexpr std::size_t n = 12;
        std::size_t len = 0;
        while (len < n && data[off + len] != 0) { ++len; }
        out.type.assign(reinterpret_cast<const char*>(data + off), len);
        off += 12;
    }
    // device_name
    if (off + (20) > length) { throw oigtl::error::ShortBufferError("device_name: short buffer"); }
    {
        constexpr std::size_t n = 20;
        std::size_t len = 0;
        while (len < n && data[off + len] != 0) { ++len; }
        out.device_name.assign(reinterpret_cast<const char*>(data + off), len);
        off += 20;
    }
    // timestamp
    if (off + (8) > length) { throw oigtl::error::ShortBufferError("timestamp: short buffer"); }
    out.timestamp = oigtl::runtime::byte_order::read_be_u64(data + off);
    off += 8;
    // body_size
    if (off + (8) > length) { throw oigtl::error::ShortBufferError("body_size: short buffer"); }
    out.body_size = oigtl::runtime::byte_order::read_be_u64(data + off);
    off += 8;
    // crc
    if (off + (8) > length) { throw oigtl::error::ShortBufferError("crc: short buffer"); }
    out.crc = oigtl::runtime::byte_order::read_be_u64(data + off);
    off += 8;
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
