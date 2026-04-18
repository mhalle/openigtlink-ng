/* GENERATED from spec/schemas/position.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * POSITION — 3D position with an optional orientation quaternion. The body carries a float32[3] position vector always, plus 0, 3, or 4 additional float32 values for orientation. Body size is exactly 12 (position only), 24 (position + 3-element compressed quaternion), or 28 (position + full 4-element quaternion). Any other body size is malformed.
 */
#ifndef OIGTL_C_MESSAGES_POSITION_H
#define OIGTL_C_MESSAGES_POSITION_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Variable body size — depends on string / array lengths at
 * runtime. Use oigtl_position_packed_size(&msg) to query. */
#define OIGTL_POSITION_BODY_SIZE_MIN  ((size_t)12)
#define OIGTL_POSITION_BODY_SIZE_MAX  ((size_t)28)

typedef struct oigtl_position {
    float position[3];
    /* view: points into wire bytes — see README for lifetime */
    const uint8_t *quaternion;
    size_t         quaternion_bytes;
} oigtl_position_t;

/* Compute the body size needed to pack `msg`. Returns the byte
 * count on success, or a negative OIGTL_ERR_* code on invalid
 * input. For fixed-body messages this is a constant. */
int oigtl_position_packed_size(const oigtl_position_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code (SHORT_BUFFER /
 * FIELD_RANGE / INVALID_ARG). */
int oigtl_position_pack(const oigtl_position_t *msg,
                              uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — valid only while *buf is valid. Copy via
 * oigtl_copy_string() / oigtl_copy_*_be() before the wire buffer
 * goes away if you need to persist them. */
int oigtl_position_unpack(const uint8_t *buf, size_t len,
                                oigtl_position_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_POSITION_H */
