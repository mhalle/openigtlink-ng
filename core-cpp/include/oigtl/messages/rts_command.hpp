// GENERATED from spec/schemas/rts_command.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// RTS_COMMAND message body codec.
#ifndef OIGTL_MESSAGES_RTS_COMMAND_HPP
#define OIGTL_MESSAGES_RTS_COMMAND_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct RtsCommand {
    static constexpr const char* kTypeId = "RTS_COMMAND";


    std::uint32_t command_id{};
    std::string command_name;
    std::uint16_t encoding{};
    std::uint32_t length{};
    std::vector<std::uint8_t> command;

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static RtsCommand unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_RTS_COMMAND_HPP
