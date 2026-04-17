// GENERATED from spec/schemas/position.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// POSITION message body codec.
#ifndef OIGTL_MESSAGES_POSITION_HPP
#define OIGTL_MESSAGES_POSITION_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Position {
    static constexpr const char* kTypeId = "POSITION";


    std::array<float, 3> position{};
    std::vector<float> quaternion;

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Position unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_POSITION_HPP
