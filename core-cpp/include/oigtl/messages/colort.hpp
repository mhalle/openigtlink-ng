// GENERATED from spec/schemas/colort.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// COLORT message body codec.
#ifndef OIGTL_MESSAGES_COLORT_HPP
#define OIGTL_MESSAGES_COLORT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Colort {
    static constexpr const char* kTypeId = "COLORT";


    std::int8_t index_type{};
    std::int8_t map_type{};
    std::vector<std::uint8_t> table;

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Colort unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_COLORT_HPP
