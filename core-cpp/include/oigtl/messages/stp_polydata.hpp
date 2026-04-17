// GENERATED from spec/schemas/stp_polydata.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// STP_POLYDATA message body codec.
#ifndef OIGTL_MESSAGES_STP_POLYDATA_HPP
#define OIGTL_MESSAGES_STP_POLYDATA_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct StpPolydata {
    static constexpr const char* kTypeId = "STP_POLYDATA";
    static constexpr std::size_t kBodySize = 0;



    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static StpPolydata unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_STP_POLYDATA_HPP
