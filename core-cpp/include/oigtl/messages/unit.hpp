// GENERATED from spec/schemas/unit.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// UNIT message body codec.
#ifndef OIGTL_MESSAGES_UNIT_HPP
#define OIGTL_MESSAGES_UNIT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Unit {
    static constexpr const char* kTypeId = "UNIT";
    static constexpr std::size_t kBodySize = 8;


    std::uint64_t packed{};

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Unit unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_UNIT_HPP
