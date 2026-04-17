// GENERATED from spec/schemas/stt_video.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// STT_VIDEO message body codec.
#ifndef OIGTL_MESSAGES_STT_VIDEO_HPP
#define OIGTL_MESSAGES_STT_VIDEO_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct SttVideo {
    static constexpr const char* kTypeId = "STT_VIDEO";
    static constexpr std::size_t kBodySize = 8;


    std::string codec;
    std::uint32_t time_interval{};

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static SttVideo unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_STT_VIDEO_HPP
