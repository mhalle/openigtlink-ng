// GENERATED from spec/schemas/stp_trans.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// STP_TRANS message body codec.
#ifndef OIGTL_MESSAGES_STP_TRANS_HPP
#define OIGTL_MESSAGES_STP_TRANS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct StpTrans {
    static constexpr const char* kTypeId = "STP_TRANS";
    static constexpr std::size_t kBodySize = 0;



    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static StpTrans unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_STP_TRANS_HPP
