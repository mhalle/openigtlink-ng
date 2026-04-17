// 58-byte OpenIGTLink message header — parse and emit.
//
// Layout (matches spec/schemas/framing_header.json and
// codec/header.py):
//   off  size  field
//     0     2  version (uint16, big-endian)
//     2    12  type_id (ascii, null-padded)
//    14    20  device_name (ascii, null-padded)
//    34     8  timestamp (uint64, big-endian)
//    42     8  body_size (uint64, big-endian)
//    50     8  crc (uint64, big-endian, ECMA-182 over body)
//   total: 58
//
// The header is invariant across protocol versions. We parse it
// before knowing which body schema to load.
#ifndef OIGTL_RUNTIME_HEADER_HPP
#define OIGTL_RUNTIME_HEADER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace oigtl::runtime {

inline constexpr std::size_t kHeaderSize = 58;

struct Header {
    std::uint16_t version = 0;
    std::string type_id;       // trimmed of trailing NULs
    std::string device_name;   // trimmed of trailing NULs
    std::uint64_t timestamp = 0;
    std::uint64_t body_size = 0;
    std::uint64_t crc = 0;
};

// Parse a 58-byte header. Throws ShortBufferError if data has < 58
// bytes available. Does NOT verify CRC (caller must, after reading
// the body).
Header unpack_header(const std::uint8_t* data, std::size_t length);

// Serialize a header into a fixed 58-byte buffer. Computes CRC over
// (body, body_length) and stores it in out[50..58]. body_size in the
// emitted header is set to body_length.
void pack_header(std::array<std::uint8_t, kHeaderSize>& out,
                 std::uint16_t version,
                 const std::string& type_id,
                 const std::string& device_name,
                 std::uint64_t timestamp,
                 const std::uint8_t* body,
                 std::size_t body_length);

// Throws CrcMismatchError if crc64(body) != header.crc.
void verify_crc(const Header& header,
                const std::uint8_t* body,
                std::size_t body_length);

}  // namespace oigtl::runtime

#endif  // OIGTL_RUNTIME_HEADER_HPP
