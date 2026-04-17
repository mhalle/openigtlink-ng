// GENERATED from spec/schemas/image.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// IMAGE message body codec.
#ifndef OIGTL_MESSAGES_IMAGE_HPP
#define OIGTL_MESSAGES_IMAGE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Image {
    static constexpr const char* kTypeId = "IMAGE";


    std::uint16_t header_version{};
    std::uint8_t num_components{};
    std::uint8_t scalar_type{};
    std::uint8_t endian{};
    std::uint8_t coord{};
    std::array<std::uint16_t, 3> size{};
    std::array<float, 12> matrix{};
    std::array<std::uint16_t, 3> subvol_offset{};
    std::array<std::uint16_t, 3> subvol_size{};
    std::vector<std::uint8_t> pixels;

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Image unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_IMAGE_HPP
