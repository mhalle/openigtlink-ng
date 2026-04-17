// GENERATED from spec/schemas/rts_ndarray.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// RTS_NDARRAY message body codec.
#ifndef OIGTL_MESSAGES_RTS_NDARRAY_HPP
#define OIGTL_MESSAGES_RTS_NDARRAY_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct RtsNdarray {
    static constexpr const char* kTypeId = "RTS_NDARRAY";
    static constexpr std::size_t kBodySize = 1;


    std::int8_t status{};

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static RtsNdarray unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_RTS_NDARRAY_HPP
