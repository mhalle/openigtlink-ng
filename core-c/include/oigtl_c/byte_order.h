/* Big-endian primitive read/write — wire-format helpers for the C codec.
 *
 * OpenIGTLink is big-endian on the wire. All 10 primitive widths
 * (u8, i8, u16, i16, u32, i32, u64, i64, f32, f64) have a matching
 * read/write pair here. Each is a `static inline` function so
 * callers pay for only the ones they reference — linker
 * `--gc-sections` discards the rest.
 *
 * No type-punning via unions or pointer casts. Floats go through
 * `memcpy` to their matching integer width, which the compiler
 * lowers to a zero-cost register move at any optimization level.
 *
 * Freestanding-friendly. C99. No platform headers required.
 */
#ifndef OIGTL_C_BYTE_ORDER_H
#define OIGTL_C_BYTE_ORDER_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 8-bit ------------------------------------------------------------- */

static inline uint8_t oigtl_read_be_u8(const uint8_t *p) {
    return p[0];
}
static inline int8_t oigtl_read_be_i8(const uint8_t *p) {
    return (int8_t)p[0];
}
static inline void oigtl_write_be_u8(uint8_t *p, uint8_t v) {
    p[0] = v;
}
static inline void oigtl_write_be_i8(uint8_t *p, int8_t v) {
    p[0] = (uint8_t)v;
}

/* ---- 16-bit ------------------------------------------------------------ */

static inline uint16_t oigtl_read_be_u16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}
static inline int16_t oigtl_read_be_i16(const uint8_t *p) {
    return (int16_t)oigtl_read_be_u16(p);
}
static inline void oigtl_write_be_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFFu);
}
static inline void oigtl_write_be_i16(uint8_t *p, int16_t v) {
    oigtl_write_be_u16(p, (uint16_t)v);
}

/* ---- 32-bit ------------------------------------------------------------ */

static inline uint32_t oigtl_read_be_u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24)
         | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] <<  8)
         |  (uint32_t)p[3];
}
static inline int32_t oigtl_read_be_i32(const uint8_t *p) {
    return (int32_t)oigtl_read_be_u32(p);
}
static inline void oigtl_write_be_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8);
    p[3] = (uint8_t) v;
}
static inline void oigtl_write_be_i32(uint8_t *p, int32_t v) {
    oigtl_write_be_u32(p, (uint32_t)v);
}

/* ---- 64-bit ------------------------------------------------------------ */

static inline uint64_t oigtl_read_be_u64(const uint8_t *p) {
    return ((uint64_t)p[0] << 56)
         | ((uint64_t)p[1] << 48)
         | ((uint64_t)p[2] << 40)
         | ((uint64_t)p[3] << 32)
         | ((uint64_t)p[4] << 24)
         | ((uint64_t)p[5] << 16)
         | ((uint64_t)p[6] <<  8)
         |  (uint64_t)p[7];
}
static inline int64_t oigtl_read_be_i64(const uint8_t *p) {
    return (int64_t)oigtl_read_be_u64(p);
}
static inline void oigtl_write_be_u64(uint8_t *p, uint64_t v) {
    p[0] = (uint8_t)(v >> 56);
    p[1] = (uint8_t)(v >> 48);
    p[2] = (uint8_t)(v >> 40);
    p[3] = (uint8_t)(v >> 32);
    p[4] = (uint8_t)(v >> 24);
    p[5] = (uint8_t)(v >> 16);
    p[6] = (uint8_t)(v >>  8);
    p[7] = (uint8_t) v;
}
static inline void oigtl_write_be_i64(uint8_t *p, int64_t v) {
    oigtl_write_be_u64(p, (uint64_t)v);
}

/* ---- floats ------------------------------------------------------------ */

/* memcpy between same-width int and float is the one type-pun the
 * C standard endorses. Every optimizer lowers it to a register move;
 * there is no runtime copy. Using it here keeps us strictly aliasing-
 * clean even under -fstrict-aliasing. */

static inline float oigtl_read_be_f32(const uint8_t *p) {
    uint32_t bits = oigtl_read_be_u32(p);
    float f;
    memcpy(&f, &bits, sizeof f);
    return f;
}
static inline void oigtl_write_be_f32(uint8_t *p, float v) {
    uint32_t bits;
    memcpy(&bits, &v, sizeof bits);
    oigtl_write_be_u32(p, bits);
}

static inline double oigtl_read_be_f64(const uint8_t *p) {
    uint64_t bits = oigtl_read_be_u64(p);
    double d;
    memcpy(&d, &bits, sizeof d);
    return d;
}
static inline void oigtl_write_be_f64(uint8_t *p, double v) {
    uint64_t bits;
    memcpy(&bits, &v, sizeof bits);
    oigtl_write_be_u64(p, bits);
}

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_BYTE_ORDER_H */
