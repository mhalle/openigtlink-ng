// GENERATED from spec/schemas/transform.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// TRANSFORM message body codec.
#ifndef OIGTL_MESSAGES_TRANSFORM_HPP
#define OIGTL_MESSAGES_TRANSFORM_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Transform {
    static constexpr const char* kTypeId = "TRANSFORM";
    static constexpr std::size_t kBodySize = 48;


    std::array<float, 12> matrix{};

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Transform unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_TRANSFORM_HPP
