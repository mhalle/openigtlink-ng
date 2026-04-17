// GENERATED from spec/schemas/status.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// STATUS message body codec.
#ifndef OIGTL_MESSAGES_STATUS_HPP
#define OIGTL_MESSAGES_STATUS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Status {
    static constexpr const char* kTypeId = "STATUS";


    std::uint16_t code{};
    std::int64_t subcode{};
    std::string error_name;
    std::string status_message;

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Status unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_STATUS_HPP
