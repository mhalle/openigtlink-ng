// GENERATED from spec/schemas/ext_header.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// EXT_HEADER message body codec.
#ifndef OIGTL_MESSAGES_EXT_HEADER_HPP
#define OIGTL_MESSAGES_EXT_HEADER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct ExtHeader {
    static constexpr const char* kTypeId = "EXT_HEADER";
    static constexpr std::size_t kBodySize = 12;


    std::uint16_t ext_header_size{};
    std::uint16_t metadata_header_size{};
    std::uint32_t metadata_size{};
    std::uint32_t message_id{};

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static ExtHeader unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_EXT_HEADER_HPP
