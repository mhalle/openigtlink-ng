// GENERATED from spec/schemas/rts_status.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// RTS_STATUS message body codec.
#ifndef OIGTL_MESSAGES_RTS_STATUS_HPP
#define OIGTL_MESSAGES_RTS_STATUS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct RtsStatus {
    static constexpr const char* kTypeId = "RTS_STATUS";
    static constexpr std::size_t kBodySize = 1;


    std::int8_t status{};

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static RtsStatus unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_RTS_STATUS_HPP
