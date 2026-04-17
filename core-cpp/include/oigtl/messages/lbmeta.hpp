// GENERATED from spec/schemas/lbmeta.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// LBMETA message body codec.
#ifndef OIGTL_MESSAGES_LBMETA_HPP
#define OIGTL_MESSAGES_LBMETA_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Lbmeta {
    static constexpr const char* kTypeId = "LBMETA";

    struct Label {
        std::string name;
        std::string device_name;
        std::uint8_t label{};
        std::uint8_t reserved{};
        std::array<std::uint8_t, 4> rgba{};
        std::array<std::uint16_t, 3> size{};
        std::string owner;
    };

    std::vector<Label> labels;

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Lbmeta unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_LBMETA_HPP
