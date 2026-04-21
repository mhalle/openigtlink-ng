#include "oigtl/runtime/header.hpp"

#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>

#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/crc64.hpp"
#include "oigtl/runtime/error.hpp"
#include "oigtl/runtime/extended_header.hpp"

namespace oigtl::runtime {

namespace {

// Read a fixed-width null-padded ASCII field, dropping anything from
// the first NUL byte onward. Matches the Python codec's behaviour
// (split(b"\x00", 1)[0]).
//
// Every byte before the first NUL must be printable ASCII (0x20–0x7E)
// per the v3 spec. The reference Python codec validates this via
// `bytes.decode("ascii")`. A byte >= 0x80 in type_id or device_name
// is always malformed — type_id is a registry key (non-ASCII can
// never match a registered type) and device_name is a
// human-readable label (non-ASCII in deployed systems is observed
// only as uninitialized memory or adversarial input). Strict
// rejection closes a cross-language divergence class without any
// observed compatibility cost.
std::string read_null_padded(const std::uint8_t* p, std::size_t n,
                             const char* field_name) {
    std::size_t len = 0;
    while (len < n && p[len] != 0) {
        if (p[len] >= 0x80) {
            std::ostringstream oss;
            oss << field_name << " contains non-ASCII byte 0x"
                << std::hex << static_cast<unsigned>(p[len])
                << " at offset " << std::dec << len;
            throw oigtl::error::MalformedMessageError(oss.str());
        }
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
    h.type_id     = read_null_padded(data + 2, 12, "type_id");
    h.device_name = read_null_padded(data + 14, 20, "device_name");
    h.timestamp   = byte_order::read_be_u64(data + 34);
    h.body_size   = byte_order::read_be_u64(data + 42);
    h.crc         = byte_order::read_be_u64(data + 50);
    return h;
}

namespace {

// Enforce the pack_header v2/v3 invariant: a body declared with
// version >= 2 must begin with a plausible extended-header region.
// Rejection reasons mirror the Python and TypeScript parallels
// verbatim so the three cores' error messages converge.
void check_v2_body_starts_with_ext_header(std::uint16_t version,
                                          const std::uint8_t* body,
                                          std::size_t body_length) {
    if (body_length < kExtendedHeaderMinSize) {
        std::ostringstream oss;
        oss << "pack_header(version=" << version
            << ") requires body to begin with a "
            << kExtendedHeaderMinSize
            << "-byte extended-header region; got " << body_length
            << " bytes. If you meant to emit v1 framing (no extended "
               "header), pass version=1 instead; if you're "
               "deliberately emitting malformed bytes from a fuzzer, "
               "pass validate=false.";
        throw std::invalid_argument(oss.str());
    }
    // Same 12-byte layout unpack_extended_header reads on receive.
    const std::uint16_t ext_header_size =
        byte_order::read_be_u16(body + 0);
    const std::uint16_t meta_header_size =
        byte_order::read_be_u16(body + 2);
    const std::uint32_t meta_size =
        byte_order::read_be_u32(body + 4);

    if (ext_header_size < kExtendedHeaderMinSize ||
            ext_header_size > body_length) {
        std::ostringstream oss;
        oss << "pack_header(version=" << version
            << "): ext_header_size " << ext_header_size
            << " is out of range [" << kExtendedHeaderMinSize
            << ", " << body_length << "].";
        throw std::invalid_argument(oss.str());
    }
    const std::size_t metadata_total =
        static_cast<std::size_t>(meta_header_size) +
        static_cast<std::size_t>(meta_size);
    if (metadata_total > body_length - ext_header_size) {
        std::ostringstream oss;
        oss << "pack_header(version=" << version
            << "): declared metadata region (" << metadata_total
            << " bytes) overruns body after the ext header ("
            << (body_length - ext_header_size) << " bytes remain).";
        throw std::invalid_argument(oss.str());
    }
}

}  // namespace

void pack_header(std::array<std::uint8_t, kHeaderSize>& out,
                 std::uint16_t version,
                 const std::string& type_id,
                 const std::string& device_name,
                 std::uint64_t timestamp,
                 const std::uint8_t* body,
                 std::size_t body_length,
                 bool validate) {
    if (validate && version >= 2) {
        check_v2_body_starts_with_ext_header(version, body, body_length);
    }
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
