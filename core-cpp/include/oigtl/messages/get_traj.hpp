// GENERATED from spec/schemas/get_traj.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// GET_TRAJ message body codec.
#ifndef OIGTL_MESSAGES_GET_TRAJ_HPP
#define OIGTL_MESSAGES_GET_TRAJ_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct GetTraj {
    static constexpr const char* kTypeId = "GET_TRAJ";
    static constexpr std::size_t kBodySize = 0;



    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static GetTraj unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_GET_TRAJ_HPP
