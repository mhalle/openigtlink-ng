// GENERATED from spec/schemas/bind.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// BIND message body codec.
#ifndef OIGTL_MESSAGES_BIND_HPP
#define OIGTL_MESSAGES_BIND_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Bind {
    static constexpr const char* kTypeId = "BIND";

    struct HeaderEntry {
        std::string type_id;
        std::uint64_t body_size{};
    };

    std::uint16_t ncmessages{};
    std::vector<HeaderEntry> header_entries;
    std::uint16_t nametable_size{};
    std::vector<std::uint8_t> name_table;
    std::vector<std::uint8_t> bodies;

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Bind unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_BIND_HPP
