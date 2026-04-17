#include "oigtl/runtime/header.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/crc64.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::runtime {

namespace {

// Read a fixed-width null-padded ASCII field, dropping anything from
// the first NUL byte onward. Matches the Python codec's behaviour
// (split(b"\x00", 1)[0]).
std::string read_null_padded(const std::uint8_t* p, std::size_t n) {
    std::size_t len = 0;
    while (len < n && p[len] != 0) {
        ++len;
    }
    return std::string(reinterpret_cast<const char*>(p), len);
}

// Write a string into a fixed-width buffer, null-padding any
// remaining bytes. Truncates if value is longer than the field.
void write_null_padded(std::uint8_t* p, std::size_t n,
                       const std::string& value) {
    const std::size_t copy_len = value.size() < n ? value.size() : n;
    std::memcpy(p, value.data(), copy_len);
    if (copy_len < n) {
        std::memset(p + copy_len, 0, n - copy_len);
    }
}

}  // namespace

Header unpack_header(const std::uint8_t* data, std::size_t length) {
    if (length < kHeaderSize) {
        std::ostringstream oss;
        oss << "header requires " << kHeaderSize
            << " bytes, got " << length;
        throw oigtl::error::ShortBufferError(oss.str());
    }
    Header h;
    h.version     = byte_order::read_be_u16(data + 0);
    if (h.version != 1 && h.version != 2 && h.version != 3) {
        std::ostringstream oss;
        oss << "header version=" << h.version
            << " is not in the supported set {1, 2, 3}";
        throw oigtl::error::MalformedMessageError(oss.str());
    }
    h.type_id     = read_null_padded(data + 2, 12);
    h.device_name = read_null_padded(data + 14, 20);
    h.timestamp   = byte_order::read_be_u64(data + 34);
    h.body_size   = byte_order::read_be_u64(data + 42);
    h.crc         = byte_order::read_be_u64(data + 50);
    return h;
}

void pack_header(std::array<std::uint8_t, kHeaderSize>& out,
                 std::uint16_t version,
                 const std::string& type_id,
                 const std::string& device_name,
                 std::uint64_t timestamp,
                 const std::uint8_t* body,
                 std::size_t body_length) {
    byte_order::write_be_u16(out.data() + 0, version);
    write_null_padded(out.data() + 2, 12, type_id);
    write_null_padded(out.data() + 14, 20, device_name);
    byte_order::write_be_u64(out.data() + 34, timestamp);
    byte_order::write_be_u64(out.data() + 42,
                             static_cast<std::uint64_t>(body_length));
    byte_order::write_be_u64(out.data() + 50, crc64(body, body_length));
}

void verify_crc(const Header& header,
                const std::uint8_t* body,
                std::size_t body_length) {
    const std::uint64_t computed = crc64(body, body_length);
    if (computed != header.crc) {
        std::ostringstream oss;
        oss << "CRC mismatch: header declares 0x" << std::hex << header.crc
            << ", body computes 0x" << computed;
        throw oigtl::error::CrcMismatchError(oss.str());
    }
}

}  // namespace oigtl::runtime
