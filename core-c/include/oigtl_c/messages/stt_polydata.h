/* GENERATED from spec/schemas/stt_polydata.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * STT_POLYDATA — Start streaming POLYDATA messages at the server's default rate.
 */
#ifndef OIGTL_C_MESSAGES_STT_POLYDATA_H
#define OIGTL_C_MESSAGES_STT_POLYDATA_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed body size — every wire-valid STT_POLYDATA has this
 * many body bytes. Useful for stack-sizing a pack buffer. */
#define OIGTL_STT_POLYDATA_BODY_SIZE ((size_t)0)

typedef struct oigtl_stt_polydata {
    /* Body-less message (header-only on the wire).
     * C99 forbids empty structs; this placeholder keeps the ABI
     * tidy without affecting pack/unpack. */
    char _placeholder;
} oigtl_stt_polydata_t;

/* Compute the body size needed to pack `msg`. Returns the byte
 * count on success, or a negative OIGTL_ERR_* code on invalid
 * input. For fixed-body messages this is a constant. */
int oigtl_stt_polydata_packed_size(const oigtl_stt_polydata_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code (SHORT_BUFFER /
 * FIELD_RANGE / INVALID_ARG). */
int oigtl_stt_polydata_pack(const oigtl_stt_polydata_t *msg,
                              uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — valid only while *buf is valid. Copy via
 * oigtl_copy_string() before the wire buffer goes away if you
 * need to persist them. */
int oigtl_stt_polydata_unpack(const uint8_t *buf, size_t len,
                                oigtl_stt_polydata_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_STT_POLYDATA_H */
