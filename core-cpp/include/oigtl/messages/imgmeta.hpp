// GENERATED from spec/schemas/imgmeta.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// IMGMETA message body codec.
#ifndef OIGTL_MESSAGES_IMGMETA_HPP
#define OIGTL_MESSAGES_IMGMETA_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Imgmeta {
    static constexpr const char* kTypeId = "IMGMETA";

    struct Image {
        std::string name;
        std::string device_name;
        std::string modality;
        std::string patient_name;
        std::string patient_id;
        std::uint64_t timestamp{};
        std::array<std::uint16_t, 3> size{};
        std::uint8_t scalar_type{};
        std::uint8_t reserved{};
    };

    std::vector<Image> images;

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Imgmeta unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_IMGMETA_HPP
