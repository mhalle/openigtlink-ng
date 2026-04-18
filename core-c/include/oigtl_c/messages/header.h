/* GENERATED from spec/schemas/header.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * HEADER — The 58-byte fixed header that precedes every OpenIGTLink message on the wire. This is protocol framing, not a message type — it has no wire type_id of its own (it IS the structure that carries the type_id). Modeled here so a generic codec can parse/emit headers from the same schema infrastructure used for message bodies.
 */
#ifndef OIGTL_C_MESSAGES_HEADER_H
#define OIGTL_C_MESSAGES_HEADER_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed body size — every wire-valid HEADER has this
 * many body bytes. Useful for stack-sizing a pack buffer. */
#define OIGTL_HEADER_BODY_SIZE ((size_t)58)

typedef struct oigtl_header {
    uint16_t version;
    char type[13];
    char device_name[21];
    uint64_t timestamp;
    uint64_t body_size;
    uint64_t crc;
} oigtl_header_t;

/* Compute the body size needed to pack `msg`. Returns the byte
 * count on success, or a negative OIGTL_ERR_* code on invalid
 * input. For fixed-body messages this is a constant. */
int oigtl_header_packed_size(const oigtl_header_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code (SHORT_BUFFER /
 * FIELD_RANGE / INVALID_ARG). */
int oigtl_header_pack(const oigtl_header_t *msg,
                              uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — valid only while *buf is valid. Copy via
 * oigtl_copy_string() before the wire buffer goes away if you
 * need to persist them. */
int oigtl_header_unpack(const uint8_t *buf, size_t len,
                                oigtl_header_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_HEADER_H */
