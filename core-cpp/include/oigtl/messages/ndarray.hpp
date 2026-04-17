// GENERATED from spec/schemas/ndarray.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// NDARRAY message body codec.
#ifndef OIGTL_MESSAGES_NDARRAY_HPP
#define OIGTL_MESSAGES_NDARRAY_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Ndarray {
    static constexpr const char* kTypeId = "NDARRAY";


    std::uint8_t scalar_type{};
    std::uint8_t dim{};
    std::vector<std::uint16_t> size;
    std::vector<std::uint8_t> data;

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Ndarray unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_NDARRAY_HPP
