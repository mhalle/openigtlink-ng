// GENERATED from spec/schemas/point.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// POINT message body codec.
#ifndef OIGTL_MESSAGES_POINT_HPP
#define OIGTL_MESSAGES_POINT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Point {
    static constexpr const char* kTypeId = "POINT";

    struct PointEntry {
        std::string name;
        std::string group_name;
        std::array<std::uint8_t, 4> rgba{};
        std::array<float, 3> position{};
        float radius{};
        std::string owner;
    };

    std::vector<PointEntry> points;

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Point unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_POINT_HPP
