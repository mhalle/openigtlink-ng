/* GENERATED from spec/schemas/qtdata.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * QTDATA — Stream of tracked-tool poses using position + quaternion representation. Each element reports one tool's name, classification, 3D position, and orientation quaternion. Typical use: real-time tracking of surgical instruments at 30–240 Hz where quaternion orientation is preferred over a full transformation matrix.
 */
#ifndef OIGTL_C_MESSAGES_QTDATA_H
#define OIGTL_C_MESSAGES_QTDATA_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Array-of-structs body: see the element struct + helpers below
 * rather than the message-level oigtl_qtdata_t. */
#define OIGTL_QTDATA_ELEMENT_SIZE ((size_t)50)

typedef struct oigtl_qtdata_element {
    char name[21];
    uint8_t type;
    uint8_t reserved;
    float position[3];
    float quaternion[4];
} oigtl_qtdata_element_t;

/* Pack one element. Returns bytes written (OIGTL_QTDATA_ELEMENT_SIZE)
 * or a negative OIGTL_ERR_* code. */
int oigtl_qtdata_element_pack(
    const oigtl_qtdata_element_t *elem,
    uint8_t *buf, size_t cap);

/* Unpack one element. Requires cap >= OIGTL_QTDATA_ELEMENT_SIZE. */
int oigtl_qtdata_element_unpack(
    const uint8_t *buf, size_t len,
    oigtl_qtdata_element_t *out);


/* Message-level view. The struct-array field is NOT materialized
 * in this struct — use the count/get helpers below to iterate
 * elements out of the wire buffer.
 *
 * Head fields (if any) are available directly. If the message has
 * only the array field, this struct is empty. */
typedef struct oigtl_qtdata {
    /* No head scalars — struct-array is the entire body. This
     * placeholder satisfies C99's prohibition on empty structs
     * without affecting pack/unpack; the real data lives in the
     * nested struct-array elements accessible via the count/get
     * helpers below. */
    char _placeholder;
} oigtl_qtdata_t;

/* Count elements in a wire body. Returns 0 on success with
 * *out_count populated; negative OIGTL_ERR_* on failure (short
 * buffer, misaligned remainder, etc.). */
int oigtl_qtdata_count(const uint8_t *buf, size_t len,
                               size_t *out_count);

/* Random access: copy the idx-th element into *out. Returns 0
 * on success, negative OIGTL_ERR_* otherwise. */
int oigtl_qtdata_get(const uint8_t *buf, size_t len,
                             size_t idx,
                             oigtl_qtdata_element_t *out);

/* Pack an array of elements into a wire body.
 * `msg` is unused for head-less messages — pass NULL.
 * Returns bytes written or a negative OIGTL_ERR_* code. */
int oigtl_qtdata_pack(const oigtl_qtdata_t *msg,
                              const oigtl_qtdata_element_t *elems,
                              size_t count,
                              uint8_t *buf, size_t cap);


#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_QTDATA_H */
