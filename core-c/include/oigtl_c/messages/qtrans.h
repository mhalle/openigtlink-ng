/* GENERATED from spec/schemas/qtrans.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * QTRANS — Position + full quaternion orientation in 28 bytes fixed. Wire-level equivalent to the 28-byte variant of POSITION, but distinct at the type_id level so receivers can dispatch deterministically without reading body_size to choose between POSITION variants. Useful for high-rate tracking where the 19% size saving over TRANSFORM matters and where senders prefer a fixed-layout message with no size-discriminated variants.
 */
#ifndef OIGTL_C_MESSAGES_QTRANS_H
#define OIGTL_C_MESSAGES_QTRANS_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed body size — every wire-valid QTRANS has this
 * many body bytes. Useful for stack-sizing a pack buffer. */
#define OIGTL_QTRANS_BODY_SIZE ((size_t)28)

typedef struct oigtl_qtrans {
    float position[3];
    float quaternion[4];
} oigtl_qtrans_t;

/* Compute the body size needed to pack `msg`. Returns the byte
 * count on success, or a negative OIGTL_ERR_* code on invalid
 * input. For fixed-body messages this is a constant. */
int oigtl_qtrans_packed_size(const oigtl_qtrans_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code (SHORT_BUFFER /
 * FIELD_RANGE / INVALID_ARG). */
int oigtl_qtrans_pack(const oigtl_qtrans_t *msg,
                              uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — valid only while *buf is valid. Copy via
 * oigtl_copy_string() before the wire buffer goes away if you
 * need to persist them. */
int oigtl_qtrans_unpack(const uint8_t *buf, size_t len,
                                oigtl_qtrans_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_QTRANS_H */
