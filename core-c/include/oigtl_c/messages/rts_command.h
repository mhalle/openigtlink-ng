/* GENERATED from spec/schemas/rts_command.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * RTS_COMMAND — Reply to a COMMAND message. Reuses the COMMAND body layout — the command_id echoes the original request's ID so the sender can correlate, and the command_name field carries an error description when the command failed. The command payload in the reply typically contains the result or diagnostic output.
 */
#ifndef OIGTL_C_MESSAGES_RTS_COMMAND_H
#define OIGTL_C_MESSAGES_RTS_COMMAND_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct oigtl_rts_command {
    uint32_t command_id;
    char command_name[129];
    uint16_t encoding;
    uint32_t length;
    /* view: points into wire bytes — see README for lifetime */
    const uint8_t *command;
    size_t         command_bytes;
} oigtl_rts_command_t;

/* Compute the body size needed to pack `msg`. Returns the byte
 * count on success, or a negative OIGTL_ERR_* code on invalid
 * input. For fixed-body messages this is a constant. */
int oigtl_rts_command_packed_size(const oigtl_rts_command_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code (SHORT_BUFFER /
 * FIELD_RANGE / INVALID_ARG). */
int oigtl_rts_command_pack(const oigtl_rts_command_t *msg,
                              uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — valid only while *buf is valid. Copy via
 * oigtl_copy_string() before the wire buffer goes away if you
 * need to persist them. */
int oigtl_rts_command_unpack(const uint8_t *buf, size_t len,
                                oigtl_rts_command_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_RTS_COMMAND_H */
