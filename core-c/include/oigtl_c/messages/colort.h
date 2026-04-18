/* GENERATED from spec/schemas/colort.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * COLORT — Color lookup table that maps integer pixel values to display colors. The body is a 2-byte header (index type + map type) followed by a raw byte array carrying the table entries. The table dimensions are fully determined by the two header fields: index type sets the number of entries (256 for uint8, 65536 for uint16) and map type sets the bytes per entry (1 for uint8, 2 for uint16, 3 for RGB).
 */
#ifndef OIGTL_C_MESSAGES_COLORT_H
#define OIGTL_C_MESSAGES_COLORT_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct oigtl_colort {
    int8_t index_type;
    int8_t map_type;
    /* view: points into wire bytes — see README for lifetime */
    const uint8_t *table;
    size_t         table_bytes;
} oigtl_colort_t;

/* Compute the body size needed to pack `msg`. Returns the byte
 * count on success, or a negative OIGTL_ERR_* code on invalid
 * input. For fixed-body messages this is a constant. */
int oigtl_colort_packed_size(const oigtl_colort_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code (SHORT_BUFFER /
 * FIELD_RANGE / INVALID_ARG). */
int oigtl_colort_pack(const oigtl_colort_t *msg,
                              uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — valid only while *buf is valid. Copy via
 * oigtl_copy_string() before the wire buffer goes away if you
 * need to persist them. */
int oigtl_colort_unpack(const uint8_t *buf, size_t len,
                                oigtl_colort_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_COLORT_H */
