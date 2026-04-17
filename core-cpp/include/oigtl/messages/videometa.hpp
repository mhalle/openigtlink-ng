// GENERATED from spec/schemas/videometa.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// VIDEOMETA message body codec.
#ifndef OIGTL_MESSAGES_VIDEOMETA_HPP
#define OIGTL_MESSAGES_VIDEOMETA_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Videometa {
    static constexpr const char* kTypeId = "VIDEOMETA";

    struct Video {
        std::string name;
        std::string device_name;
        std::string patient_name;
        std::string patient_id;
        std::int16_t zoom_level{};
        double focal_length{};
        std::array<std::uint16_t, 3> size{};
        std::array<float, 12> matrix{};
        std::uint8_t scalar_type{};
        std::uint8_t reserved{};
    };

    std::vector<Video> videos;

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Videometa unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_VIDEOMETA_HPP
