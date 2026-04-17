// Big-endian primitive read/write — wire-format helpers.
//
// OpenIGTLink is big-endian on the wire. We keep logical values in
// host order and convert explicitly at pack/unpack time. No
// type-punning, no #pragma pack, no struct casting — every multibyte
// read/write goes through one of these inline helpers.
//
// Header-only. Standard library only.
#ifndef OIGTL_RUNTIME_BYTE_ORDER_HPP
#define OIGTL_RUNTIME_BYTE_ORDER_HPP

#include <cstdint>
#include <cstring>

namespace oigtl::runtime::byte_order {

// Single-byte helpers for symmetry with multi-byte ones — codegen
// can dispatch on primitive type without special-casing 1-byte reads.
inline std::uint8_t read_be_u8(const std::uint8_t* p) noexcept {
    return p[0];
}
inline std::int8_t read_be_i8(const std::uint8_t* p) noexcept {
    return static_cast<std::int8_t>(p[0]);
}
inline void write_be_u8(std::uint8_t* p, std::uint8_t v) noexcept {
    p[0] = v;
}
inline void write_be_i8(std::uint8_t* p, std::int8_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v);
}

inline std::uint16_t read_be_u16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>((std::uint16_t{p[0]} << 8) | p[1]);
}

inline std::uint32_t read_be_u32(const std::uint8_t* p) noexcept {
    return (std::uint32_t{p[0]} << 24) | (std::uint32_t{p[1]} << 16)
         | (std::uint32_t{p[2]} << 8)  |  std::uint32_t{p[3]};
}

inline std::uint64_t read_be_u64(const std::uint8_t* p) noexcept {
    return (std::uint64_t{p[0]} << 56) | (std::uint64_t{p[1]} << 48)
         | (std::uint64_t{p[2]} << 40) | (std::uint64_t{p[3]} << 32)
         | (std::uint64_t{p[4]} << 24) | (std::uint64_t{p[5]} << 16)
         | (std::uint64_t{p[6]} << 8)  |  std::uint64_t{p[7]};
}

inline std::int16_t read_be_i16(const std::uint8_t* p) noexcept {
    return static_cast<std::int16_t>(read_be_u16(p));
}
inline std::int32_t read_be_i32(const std::uint8_t* p) noexcept {
    return static_cast<std::int32_t>(read_be_u32(p));
}
inline std::int64_t read_be_i64(const std::uint8_t* p) noexcept {
    return static_cast<std::int64_t>(read_be_u64(p));
}

inline float read_be_f32(const std::uint8_t* p) noexcept {
    const std::uint32_t bits = read_be_u32(p);
    float out;
    std::memcpy(&out, &bits, sizeof(out));
    return out;
}

inline double read_be_f64(const std::uint8_t* p) noexcept {
    const std::uint64_t bits = read_be_u64(p);
    double out;
    std::memcpy(&out, &bits, sizeof(out));
    return out;
}

inline void write_be_u16(std::uint8_t* p, std::uint16_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v >> 8);
    p[1] = static_cast<std::uint8_t>(v);
}

inline void write_be_u32(std::uint8_t* p, std::uint32_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v >> 24);
    p[1] = static_cast<std::uint8_t>(v >> 16);
    p[2] = static_cast<std::uint8_t>(v >> 8);
    p[3] = static_cast<std::uint8_t>(v);
}

inline void write_be_u64(std::uint8_t* p, std::uint64_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v >> 56);
    p[1] = static_cast<std::uint8_t>(v >> 48);
    p[2] = static_cast<std::uint8_t>(v >> 40);
    p[3] = static_cast<std::uint8_t>(v >> 32);
    p[4] = static_cast<std::uint8_t>(v >> 24);
    p[5] = static_cast<std::uint8_t>(v >> 16);
    p[6] = static_cast<std::uint8_t>(v >> 8);
    p[7] = static_cast<std::uint8_t>(v);
}

inline void write_be_i16(std::uint8_t* p, std::int16_t v) noexcept {
    write_be_u16(p, static_cast<std::uint16_t>(v));
}
inline void write_be_i32(std::uint8_t* p, std::int32_t v) noexcept {
    write_be_u32(p, static_cast<std::uint32_t>(v));
}
inline void write_be_i64(std::uint8_t* p, std::int64_t v) noexcept {
    write_be_u64(p, static_cast<std::uint64_t>(v));
}

inline void write_be_f32(std::uint8_t* p, float v) noexcept {
    std::uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    write_be_u32(p, bits);
}

inline void write_be_f64(std::uint8_t* p, double v) noexcept {
    std::uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    write_be_u64(p, bits);
}

}  // namespace oigtl::runtime::byte_order

#endif  // OIGTL_RUNTIME_BYTE_ORDER_HPP
