// GENERATED from spec/schemas/metadata.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// METADATA message body codec.
#ifndef OIGTL_MESSAGES_METADATA_HPP
#define OIGTL_MESSAGES_METADATA_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Metadata {
    static constexpr const char* kTypeId = "METADATA";

    struct IndexEntry {
        std::uint16_t key_size{};
        std::uint16_t value_encoding{};
        std::uint32_t value_size{};
    };

    std::uint16_t count{};
    std::vector<IndexEntry> index_entries;
    std::vector<std::uint8_t> body;

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Metadata unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_METADATA_HPP
