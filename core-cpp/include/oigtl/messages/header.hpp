// GENERATED from spec/schemas/header.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// HEADER message body codec.
#ifndef OIGTL_MESSAGES_HEADER_HPP
#define OIGTL_MESSAGES_HEADER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Header {
    static constexpr const char* kTypeId = "HEADER";
    static constexpr std::size_t kBodySize = 58;


    std::uint16_t version{};
    std::string type;
    std::string device_name;
    std::uint64_t timestamp{};
    std::uint64_t body_size{};
    std::uint64_t crc{};

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Header unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_HEADER_HPP
