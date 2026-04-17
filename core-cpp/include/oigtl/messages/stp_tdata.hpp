// GENERATED from spec/schemas/stp_tdata.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// STP_TDATA message body codec.
#ifndef OIGTL_MESSAGES_STP_TDATA_HPP
#define OIGTL_MESSAGES_STP_TDATA_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct StpTdata {
    static constexpr const char* kTypeId = "STP_TDATA";
    static constexpr std::size_t kBodySize = 0;



    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static StpTdata unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_STP_TDATA_HPP
