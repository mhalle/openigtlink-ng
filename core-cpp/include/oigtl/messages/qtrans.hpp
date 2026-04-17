// GENERATED from spec/schemas/qtrans.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// QTRANS message body codec.
#ifndef OIGTL_MESSAGES_QTRANS_HPP
#define OIGTL_MESSAGES_QTRANS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Qtrans {
    static constexpr const char* kTypeId = "QTRANS";
    static constexpr std::size_t kBodySize = 28;


    std::array<float, 3> position{};
    std::array<float, 4> quaternion{};

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Qtrans unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_QTRANS_HPP
