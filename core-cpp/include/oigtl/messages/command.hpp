// GENERATED from spec/schemas/command.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// COMMAND message body codec.
#ifndef OIGTL_MESSAGES_COMMAND_HPP
#define OIGTL_MESSAGES_COMMAND_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Command {
    static constexpr const char* kTypeId = "COMMAND";


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
    static Command unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_COMMAND_HPP
