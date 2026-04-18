/* GENERATED from spec/schemas/get_image.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * GET_IMAGE — Request the current IMAGE for the device named in the header. If the header device name is empty, the server returns a default image.
 */
#ifndef OIGTL_C_MESSAGES_GET_IMAGE_H
#define OIGTL_C_MESSAGES_GET_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed body size — every wire-valid GET_IMAGE has this
 * many body bytes. Useful for stack-sizing a pack buffer. */
#define OIGTL_GET_IMAGE_BODY_SIZE ((size_t)0)

typedef struct oigtl_get_image {
    /* Body-less message (header-only on the wire).
     * C99 forbids empty structs; this placeholder keeps the ABI
     * tidy without affecting pack/unpack. */
    char _placeholder;
} oigtl_get_image_t;

/* Compute the body size needed to pack `msg`. Returns the byte
 * count on success, or a negative OIGTL_ERR_* code on invalid
 * input. For fixed-body messages this is a constant. */
int oigtl_get_image_packed_size(const oigtl_get_image_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code (SHORT_BUFFER /
 * FIELD_RANGE / INVALID_ARG). */
int oigtl_get_image_pack(const oigtl_get_image_t *msg,
                              uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — valid only while *buf is valid. Copy via
 * oigtl_copy_string() before the wire buffer goes away if you
 * need to persist them. */
int oigtl_get_image_unpack(const uint8_t *buf, size_t len,
                                oigtl_get_image_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_GET_IMAGE_H */
