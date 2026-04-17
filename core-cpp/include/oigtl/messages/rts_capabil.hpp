// GENERATED from spec/schemas/rts_capabil.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// RTS_CAPABIL message body codec.
#ifndef OIGTL_MESSAGES_RTS_CAPABIL_HPP
#define OIGTL_MESSAGES_RTS_CAPABIL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct RtsCapabil {
    static constexpr const char* kTypeId = "RTS_CAPABIL";
    static constexpr std::size_t kBodySize = 1;


    std::int8_t status{};

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static RtsCapabil unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_RTS_CAPABIL_HPP
