/* GENERATED from spec/schemas/rts_status.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * RTS_STATUS — Server's return status for a STATUS query (GET/STT/STP). Per Documents/Protocol/query.md, every message type has an RTS_ form for error returns. A single int8: 0 = success, 1 = error.
 */
#ifndef OIGTL_C_MESSAGES_RTS_STATUS_H
#define OIGTL_C_MESSAGES_RTS_STATUS_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed body size — every wire-valid RTS_STATUS has this
 * many body bytes. Useful for stack-sizing a pack buffer. */
#define OIGTL_RTS_STATUS_BODY_SIZE ((size_t)1)

typedef struct oigtl_rts_status {
    int8_t status;
} oigtl_rts_status_t;

/* Compute the body size needed to pack `msg`. Returns the byte
 * count on success, or a negative OIGTL_ERR_* code on invalid
 * input. For fixed-body messages this is a constant. */
int oigtl_rts_status_packed_size(const oigtl_rts_status_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code (SHORT_BUFFER /
 * FIELD_RANGE / INVALID_ARG). */
int oigtl_rts_status_pack(const oigtl_rts_status_t *msg,
                              uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — valid only while *buf is valid. Copy via
 * oigtl_copy_string() / oigtl_copy_*_be() before the wire buffer
 * goes away if you need to persist them. */
int oigtl_rts_status_unpack(const uint8_t *buf, size_t len,
                                oigtl_rts_status_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_RTS_STATUS_H */
