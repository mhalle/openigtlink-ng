// GENERATED from spec/schemas/stt_qtrans.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// STT_QTRANS message body codec.
#ifndef OIGTL_MESSAGES_STT_QTRANS_HPP
#define OIGTL_MESSAGES_STT_QTRANS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct SttQtrans {
    static constexpr const char* kTypeId = "STT_QTRANS";
    static constexpr std::size_t kBodySize = 0;



    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static SttQtrans unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_STT_QTRANS_HPP
