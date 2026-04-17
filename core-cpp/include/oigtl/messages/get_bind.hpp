// GENERATED from spec/schemas/get_bind.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// GET_BIND message body codec.
#ifndef OIGTL_MESSAGES_GET_BIND_HPP
#define OIGTL_MESSAGES_GET_BIND_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct GetBind {
    static constexpr const char* kTypeId = "GET_BIND";


    std::uint16_t ncmessages{};
    std::vector<std::string> type_ids;
    std::uint16_t nametable_size{};
    std::vector<std::uint8_t> name_table;

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static GetBind unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_GET_BIND_HPP
