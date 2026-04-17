// GENERATED from spec/schemas/tdata.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// TDATA message body codec.
#ifndef OIGTL_MESSAGES_TDATA_HPP
#define OIGTL_MESSAGES_TDATA_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Tdata {
    static constexpr const char* kTypeId = "TDATA";

    struct Tool {
        std::string name;
        std::uint8_t type{};
        std::uint8_t reserved{};
        std::array<float, 12> transform{};
    };

    std::vector<Tool> tools;

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Tdata unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_TDATA_HPP
