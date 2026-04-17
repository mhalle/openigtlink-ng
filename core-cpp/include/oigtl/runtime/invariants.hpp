// Cross-field post-unpack invariants.
//
// For a handful of message types, the spec defines constraints that
// span multiple fields — e.g. NDARRAY requires
// ``len(data) == product(size) × bytes_per_scalar(scalar_type)``.
// These cannot be expressed through per-field type annotations, so
// schemas name them via ``post_unpack_invariant`` and every codec
// runtime implements the same validator by name.
//
// The canonical reference lives in corpus-tools/.../codec/policy.py::
// POST_UNPACK_INVARIANTS. Python / TS / C++ all mirror it; the
// differential fuzzer holds the four implementations in sync. Adding
// a new invariant requires touching all four.
//
// Generated code dispatches here via
// ``oigtl::runtime::invariants::apply("name", msg)`` — see the
// per-message ``unpack`` in src/messages/*.cpp.

#ifndef OIGTL_RUNTIME_INVARIANTS_HPP
#define OIGTL_RUNTIME_INVARIANTS_HPP

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "oigtl/runtime/error.hpp"

namespace oigtl::runtime::invariants {

namespace detail {

// Scalar-type → bytes-per-element lookup. 0 return means "invalid".
// NDARRAY accepts complex (13) on top of the IMAGE set.
inline std::size_t ndarray_bytes_per_scalar(int st) noexcept {
    switch (st) {
        case 2: case 3: return 1;
        case 4: case 5: return 2;
        case 6: case 7: case 10: return 4;
        case 11: return 8;
        case 13: return 16;
        default: return 0;
    }
}

inline std::size_t image_bytes_per_scalar(int st) noexcept {
    switch (st) {
        case 2: case 3: return 1;
        case 4: case 5: return 2;
        case 6: case 7: case 10: return 4;
        case 11: return 8;
        default: return 0;
    }
}

template <typename Seq>
inline std::uint64_t product_u64(const Seq& seq) noexcept {
    std::uint64_t p = 1;
    for (auto v : seq) p *= static_cast<std::uint64_t>(v);
    return p;
}

}  // namespace detail


// NDARRAY: scalar_type ∈ {2,3,4,5,6,7,10,11,13} and
// len(data) == product(size) × bytes_per_scalar(scalar_type).
//
// The Msg template parameter is the generated message struct; it
// must expose scalar_type, size, data accessible as member fields.
template <typename Msg>
inline void check_ndarray(const Msg& msg) {
    const auto bps = detail::ndarray_bytes_per_scalar(
        static_cast<int>(msg.scalar_type));
    if (bps == 0) {
        std::ostringstream oss;
        oss << "NDARRAY: invalid scalar_type="
            << static_cast<int>(msg.scalar_type);
        throw oigtl::error::MalformedMessageError(oss.str());
    }
    const auto prod = detail::product_u64(msg.size);
    const auto expected = prod * bps;
    if (msg.data.size() != expected) {
        std::ostringstream oss;
        oss << "NDARRAY: data length " << msg.data.size()
            << " does not match product(size)=" << prod
            << " × bytes_per_scalar(" << static_cast<int>(msg.scalar_type)
            << ")=" << bps << " = " << expected;
        throw oigtl::error::MalformedMessageError(oss.str());
    }
}


// IMAGE: scalar_type ∈ {2,3,4,5,6,7,10,11}, endian ∈ {1,2,3},
// coord ∈ {1,2}, subvol_offset[i]+subvol_size[i] ≤ size[i],
// len(pixels) == product(subvol_size) × num_components × bytes_per_scalar.
template <typename Msg>
inline void check_image(const Msg& msg) {
    const auto bps = detail::image_bytes_per_scalar(
        static_cast<int>(msg.scalar_type));
    if (bps == 0) {
        std::ostringstream oss;
        oss << "IMAGE: invalid scalar_type="
            << static_cast<int>(msg.scalar_type);
        throw oigtl::error::MalformedMessageError(oss.str());
    }
    const int endian = static_cast<int>(msg.endian);
    if (endian < 1 || endian > 3) {
        std::ostringstream oss;
        oss << "IMAGE: invalid endian=" << endian;
        throw oigtl::error::MalformedMessageError(oss.str());
    }
    const int coord = static_cast<int>(msg.coord);
    if (coord < 1 || coord > 2) {
        std::ostringstream oss;
        oss << "IMAGE: invalid coord=" << coord;
        throw oigtl::error::MalformedMessageError(oss.str());
    }
    for (std::size_t i = 0; i < msg.size.size(); ++i) {
        const auto off = static_cast<std::uint64_t>(msg.subvol_offset[i]);
        const auto sub = static_cast<std::uint64_t>(msg.subvol_size[i]);
        const auto whole = static_cast<std::uint64_t>(msg.size[i]);
        if (off + sub > whole) {
            std::ostringstream oss;
            oss << "IMAGE: subvol_offset[" << i << "]+subvol_size[" << i
                << "]=" << off << "+" << sub
                << " exceeds size[" << i << "]=" << whole;
            throw oigtl::error::MalformedMessageError(oss.str());
        }
    }
    const auto prod = detail::product_u64(msg.subvol_size);
    const auto expected = prod
        * static_cast<std::uint64_t>(msg.num_components) * bps;
    if (msg.pixels.size() != expected) {
        std::ostringstream oss;
        oss << "IMAGE: pixels length " << msg.pixels.size()
            << " does not match product(subvol_size)=" << prod
            << " × num_components=" << static_cast<int>(msg.num_components)
            << " × bytes_per_scalar("
            << static_cast<int>(msg.scalar_type) << ")=" << bps
            << " = " << expected;
        throw oigtl::error::MalformedMessageError(oss.str());
    }
}

}  // namespace oigtl::runtime::invariants

#endif  // OIGTL_RUNTIME_INVARIANTS_HPP
