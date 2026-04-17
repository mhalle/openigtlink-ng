// GENERATED from spec/schemas/get_vmeta.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// GET_VMETA message body codec.
#ifndef OIGTL_MESSAGES_GET_VMETA_HPP
#define OIGTL_MESSAGES_GET_VMETA_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct GetVmeta {
    static constexpr const char* kTypeId = "GET_VMETA";
    static constexpr std::size_t kBodySize = 0;



    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static GetVmeta unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_GET_VMETA_HPP
