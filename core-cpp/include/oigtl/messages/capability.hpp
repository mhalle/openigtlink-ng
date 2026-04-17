// GENERATED from spec/schemas/capability.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// CAPABILITY message body codec.
#ifndef OIGTL_MESSAGES_CAPABILITY_HPP
#define OIGTL_MESSAGES_CAPABILITY_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Capability {
    static constexpr const char* kTypeId = "CAPABILITY";


    std::vector<std::string> supported_types;

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Capability unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_CAPABILITY_HPP
