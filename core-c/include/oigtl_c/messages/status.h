/* GENERATED from spec/schemas/status.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * STATUS — Reports the operational status of a device or the outcome of a request. Carries a numeric status code, a device-specific subcode, a short error name, and an optional free-text status message.
 */
#ifndef OIGTL_C_MESSAGES_STATUS_H
#define OIGTL_C_MESSAGES_STATUS_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Variable body size — depends on string / array lengths at
 * runtime. Use oigtl_status_packed_size(&msg) to query. */

typedef struct oigtl_status {
    uint16_t code;
    int64_t subcode;
    char error_name[21];
    /* view: points into wire bytes — see README for lifetime */
    const char *status_message;
    size_t      status_message_len;
} oigtl_status_t;

/* Compute the body size needed to pack `msg`. Returns the byte
 * count on success, or a negative OIGTL_ERR_* code on invalid
 * input. For fixed-body messages this is a constant. */
int oigtl_status_packed_size(const oigtl_status_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code (SHORT_BUFFER /
 * FIELD_RANGE / INVALID_ARG). */
int oigtl_status_pack(const oigtl_status_t *msg,
                              uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — valid only while *buf is valid. Copy via
 * oigtl_copy_string() / oigtl_copy_*_be() before the wire buffer
 * goes away if you need to persist them. */
int oigtl_status_unpack(const uint8_t *buf, size_t len,
                                oigtl_status_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_STATUS_H */
