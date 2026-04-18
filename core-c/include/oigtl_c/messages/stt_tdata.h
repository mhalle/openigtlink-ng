/* GENERATED from spec/schemas/stt_tdata.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * STT_TDATA — Start streaming TDATA (tracking data) messages at a specified update interval, in a named coordinate system. The server responds with periodic TDATA messages until a STP_TDATA is received.
 */
#ifndef OIGTL_C_MESSAGES_STT_TDATA_H
#define OIGTL_C_MESSAGES_STT_TDATA_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed body size — every wire-valid STT_TDATA has this
 * many body bytes. Useful for stack-sizing a pack buffer. */
#define OIGTL_STT_TDATA_BODY_SIZE ((size_t)36)

typedef struct oigtl_stt_tdata {
    int32_t resolution;
    char coord_name[33];
} oigtl_stt_tdata_t;

/* Compute the body size needed to pack `msg`. Returns the byte
 * count on success, or a negative OIGTL_ERR_* code on invalid
 * input. For fixed-body messages this is a constant. */
int oigtl_stt_tdata_packed_size(const oigtl_stt_tdata_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code (SHORT_BUFFER /
 * FIELD_RANGE / INVALID_ARG). */
int oigtl_stt_tdata_pack(const oigtl_stt_tdata_t *msg,
                              uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — valid only while *buf is valid. Copy via
 * oigtl_copy_string() before the wire buffer goes away if you
 * need to persist them. */
int oigtl_stt_tdata_unpack(const uint8_t *buf, size_t len,
                                oigtl_stt_tdata_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_STT_TDATA_H */
