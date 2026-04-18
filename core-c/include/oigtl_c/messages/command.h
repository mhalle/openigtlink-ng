/* GENERATED from spec/schemas/command.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * COMMAND — Carries a structured command — typically an XML document — from one peer to another, tagged with a session-unique ID and a symbolic name so replies (RTS_COMMAND) can reference it. The body is a 138-byte fixed header followed by a variable-length command string whose byte count is declared in the header's `length` field and whose character encoding is declared by the `encoding` field (an IANA MIBenum value).
 */
#ifndef OIGTL_C_MESSAGES_COMMAND_H
#define OIGTL_C_MESSAGES_COMMAND_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Variable body size — depends on string / array lengths at
 * runtime. Use oigtl_command_packed_size(&msg) to query. */

typedef struct oigtl_command {
    uint32_t command_id;
    char command_name[129];
    uint16_t encoding;
    uint32_t length;
    /* view: points into wire bytes — see README for lifetime */
    const uint8_t *command;
    size_t         command_bytes;
} oigtl_command_t;

/* Compute the body size needed to pack `msg`. Returns the byte
 * count on success, or a negative OIGTL_ERR_* code on invalid
 * input. For fixed-body messages this is a constant. */
int oigtl_command_packed_size(const oigtl_command_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code (SHORT_BUFFER /
 * FIELD_RANGE / INVALID_ARG). */
int oigtl_command_pack(const oigtl_command_t *msg,
                              uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — valid only while *buf is valid. Copy via
 * oigtl_copy_string() / oigtl_copy_*_be() before the wire buffer
 * goes away if you need to persist them. */
int oigtl_command_unpack(const uint8_t *buf, size_t len,
                                oigtl_command_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_COMMAND_H */
