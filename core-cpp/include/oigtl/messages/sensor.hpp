// GENERATED from spec/schemas/sensor.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// SENSOR message body codec.
#ifndef OIGTL_MESSAGES_SENSOR_HPP
#define OIGTL_MESSAGES_SENSOR_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Sensor {
    static constexpr const char* kTypeId = "SENSOR";


    std::uint8_t larray{};
    std::uint8_t status{};
    std::uint64_t unit{};
    std::vector<double> data;

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Sensor unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_SENSOR_HPP
