/* GENERATED from spec/schemas/ndarray.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * NDARRAY — Variable-rank N-dimensional numerical array. The body carries a 2-byte fixed header (scalar type + dimension count), a variable-length size table of uint16 values (one per axis), and then the raw array data as bytes. The data section contains product(size[0..dim-1]) elements of the declared scalar type, in row-major (C-contiguous) order.
 */
#ifndef OIGTL_C_MESSAGES_NDARRAY_H
#define OIGTL_C_MESSAGES_NDARRAY_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct oigtl_ndarray {
    uint8_t scalar_type;
    uint8_t dim;
    /* view: points into wire bytes — see README for lifetime */
    const uint8_t *size;
    size_t         size_bytes;
    /* view: points into wire bytes — see README for lifetime */
    const uint8_t *data;
    size_t         data_bytes;
} oigtl_ndarray_t;

/* Compute the body size needed to pack `msg`. Returns the byte
 * count on success, or a negative OIGTL_ERR_* code on invalid
 * input. For fixed-body messages this is a constant. */
int oigtl_ndarray_packed_size(const oigtl_ndarray_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code (SHORT_BUFFER /
 * FIELD_RANGE / INVALID_ARG). */
int oigtl_ndarray_pack(const oigtl_ndarray_t *msg,
                              uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — valid only while *buf is valid. Copy via
 * oigtl_copy_string() before the wire buffer goes away if you
 * need to persist them. */
int oigtl_ndarray_unpack(const uint8_t *buf, size_t len,
                                oigtl_ndarray_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_NDARRAY_H */
