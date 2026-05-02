/* GENERATED from spec/schemas/colortable.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * COLORTABLE — Legacy wire encoding of the color-lookup-table message. Body layout is identical to the modern `COLORT` type_id; only the 12-byte type-id field differs — the legacy form spells the type as 'COLORTABLE' (10 chars), predating upstream's shortening to 'COLORT' (6 chars). Kept as a distinct schema so receivers can round-trip pre-shortening traffic without ambiguity.
 */
#ifndef OIGTL_C_MESSAGES_COLORTABLE_H
#define OIGTL_C_MESSAGES_COLORTABLE_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct oigtl_colortable {
    int8_t index_type;
    int8_t map_type;
    /* view: points into wire bytes — see README for lifetime */
    const uint8_t *table;
    size_t         table_bytes;
} oigtl_colortable_t;

/* Compute the body size needed to pack `msg`. Returns the byte
 * count on success, or a negative OIGTL_ERR_* code on invalid
 * input. For fixed-body messages this is a constant. */
int oigtl_colortable_packed_size(const oigtl_colortable_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code (SHORT_BUFFER /
 * FIELD_RANGE / INVALID_ARG). */
int oigtl_colortable_pack(const oigtl_colortable_t *msg,
                              uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — valid only while *buf is valid. Copy via
 * oigtl_copy_string() before the wire buffer goes away if you
 * need to persist them. */
int oigtl_colortable_unpack(const uint8_t *buf, size_t len,
                                oigtl_colortable_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_COLORTABLE_H */
