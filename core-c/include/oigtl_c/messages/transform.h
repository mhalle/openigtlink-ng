/* GENERATED from spec/schemas/transform.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * TRANSFORM — 4x4 homogeneous transformation matrix in a right-handed coordinate system. Used to communicate a single rigid-body pose — for example, a tracked tool's position and orientation.
 */
#ifndef OIGTL_C_MESSAGES_TRANSFORM_H
#define OIGTL_C_MESSAGES_TRANSFORM_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed body size — every wire-valid TRANSFORM has this
 * many body bytes. Useful for stack-sizing a pack buffer. */
#define OIGTL_TRANSFORM_BODY_SIZE ((size_t)48)

typedef struct oigtl_transform {
    float matrix[12];
} oigtl_transform_t;

/* Compute the body size needed to pack `msg`. Returns the byte
 * count on success, or a negative OIGTL_ERR_* code on invalid
 * input. For fixed-body messages this is a constant. */
int oigtl_transform_packed_size(const oigtl_transform_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code (SHORT_BUFFER /
 * FIELD_RANGE / INVALID_ARG). */
int oigtl_transform_pack(const oigtl_transform_t *msg,
                              uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — valid only while *buf is valid. Copy via
 * oigtl_copy_string() / oigtl_copy_*_be() before the wire buffer
 * goes away if you need to persist them. */
int oigtl_transform_unpack(const uint8_t *buf, size_t len,
                                oigtl_transform_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_TRANSFORM_H */
