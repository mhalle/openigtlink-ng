/* GENERATED from spec/schemas/tdata.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * TDATA — Stream of tracked-tool poses using a 3×4 transformation matrix per tool. Each element reports one tool's name, classification, and full pose as 12 floats in the same column-major layout as TRANSFORM. Typical use: real-time tracking of surgical instruments at 30–240 Hz when the sender prefers matrix orientation over quaternion.
 */
#ifndef OIGTL_C_MESSAGES_TDATA_H
#define OIGTL_C_MESSAGES_TDATA_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Array-of-structs body: see the element struct + helpers below
 * rather than the message-level oigtl_tdata_t. */
#define OIGTL_TDATA_ELEMENT_SIZE ((size_t)70)

typedef struct oigtl_tdata_element {
    char name[21];
    uint8_t type;
    uint8_t reserved;
    float transform[12];
} oigtl_tdata_element_t;

/* Pack one element. Returns bytes written (OIGTL_TDATA_ELEMENT_SIZE)
 * or a negative OIGTL_ERR_* code. */
int oigtl_tdata_element_pack(
    const oigtl_tdata_element_t *elem,
    uint8_t *buf, size_t cap);

/* Unpack one element. Requires cap >= OIGTL_TDATA_ELEMENT_SIZE. */
int oigtl_tdata_element_unpack(
    const uint8_t *buf, size_t len,
    oigtl_tdata_element_t *out);


/* Message-level view. The struct-array field is NOT materialized
 * in this struct — use the count/get helpers below to iterate
 * elements out of the wire buffer.
 *
 * Head fields (if any) are available directly. If the message has
 * only the array field, this struct is empty. */
typedef struct oigtl_tdata {
    /* No head scalars — struct-array is the entire body. This
     * placeholder satisfies C99's prohibition on empty structs
     * without affecting pack/unpack; the real data lives in the
     * nested struct-array elements accessible via the count/get
     * helpers below. */
    char _placeholder;
} oigtl_tdata_t;

/* Count elements in a wire body. Returns 0 on success with
 * *out_count populated; negative OIGTL_ERR_* on failure (short
 * buffer, misaligned remainder, etc.). */
int oigtl_tdata_count(const uint8_t *buf, size_t len,
                               size_t *out_count);

/* Random access: copy the idx-th element into *out. Returns 0
 * on success, negative OIGTL_ERR_* otherwise. */
int oigtl_tdata_get(const uint8_t *buf, size_t len,
                             size_t idx,
                             oigtl_tdata_element_t *out);

/* Pack an array of elements into a wire body.
 * `msg` is unused for head-less messages — pass NULL.
 * Returns bytes written or a negative OIGTL_ERR_* code. */
int oigtl_tdata_pack(const oigtl_tdata_t *msg,
                              const oigtl_tdata_element_t *elems,
                              size_t count,
                              uint8_t *buf, size_t cap);


#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_TDATA_H */
