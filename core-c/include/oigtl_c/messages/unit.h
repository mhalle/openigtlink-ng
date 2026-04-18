/* GENERATED from spec/schemas/unit.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * UNIT — Physical unit encoding — a 8-byte (uint64) packed representation of an SI unit with a metric prefix. This is a field-level encoding used inside SENSOR messages (and potentially other message types), not a standalone wire message type. Modeled here so a codec can interpret the `unit` field in SENSOR's element struct. The uint64 packs a 4-bit prefix, up to 6 base/derived unit codes (6 bits each), and 6 signed exponents (4 bits each, range [-7, 7]).
 */
#ifndef OIGTL_C_MESSAGES_UNIT_H
#define OIGTL_C_MESSAGES_UNIT_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed body size — every wire-valid UNIT has this
 * many body bytes. Useful for stack-sizing a pack buffer. */
#define OIGTL_UNIT_BODY_SIZE ((size_t)8)

typedef struct oigtl_unit {
    uint64_t packed;
} oigtl_unit_t;

/* Compute the body size needed to pack `msg`. Returns the byte
 * count on success, or a negative OIGTL_ERR_* code on invalid
 * input. For fixed-body messages this is a constant. */
int oigtl_unit_packed_size(const oigtl_unit_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code (SHORT_BUFFER /
 * FIELD_RANGE / INVALID_ARG). */
int oigtl_unit_pack(const oigtl_unit_t *msg,
                              uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — valid only while *buf is valid. Copy via
 * oigtl_copy_string() before the wire buffer goes away if you
 * need to persist them. */
int oigtl_unit_unpack(const uint8_t *buf, size_t len,
                                oigtl_unit_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_UNIT_H */
