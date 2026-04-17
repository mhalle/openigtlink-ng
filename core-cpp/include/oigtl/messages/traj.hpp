// GENERATED from spec/schemas/traj.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// TRAJ message body codec.
#ifndef OIGTL_MESSAGES_TRAJ_HPP
#define OIGTL_MESSAGES_TRAJ_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Traj {
    static constexpr const char* kTypeId = "TRAJ";

    struct Trajectory {
        std::string name;
        std::string group_name;
        std::int8_t type{};
        std::int8_t reserved{};
        std::array<std::uint8_t, 4> rgba{};
        std::array<float, 3> entry_pos{};
        std::array<float, 3> target_pos{};
        float radius{};
        std::string owner_name;
    };

    std::vector<Trajectory> trajectories;

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Traj unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_TRAJ_HPP
