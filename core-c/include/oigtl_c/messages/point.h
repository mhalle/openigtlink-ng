/* GENERATED from spec/schemas/point.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * POINT — List of 3D annotated points: landmarks, fiducials, surgical targets, and similar discrete spatial markers. Each element carries a point's name, group, color, 3D position, radius, and owning-image reference.
 */
#ifndef OIGTL_C_MESSAGES_POINT_H
#define OIGTL_C_MESSAGES_POINT_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Array-of-structs body: see the element struct + helpers below
 * rather than the message-level oigtl_point_t. */
#define OIGTL_POINT_ELEMENT_SIZE ((size_t)136)

typedef struct oigtl_point_element {
    char name[65];
    char group_name[33];
    uint8_t rgba[4];
    float position[3];
    float radius;
    char owner[21];
} oigtl_point_element_t;

/* Pack one element. Returns bytes written (OIGTL_POINT_ELEMENT_SIZE)
 * or a negative OIGTL_ERR_* code. */
int oigtl_point_element_pack(
    const oigtl_point_element_t *elem,
    uint8_t *buf, size_t cap);

/* Unpack one element. Requires cap >= OIGTL_POINT_ELEMENT_SIZE. */
int oigtl_point_element_unpack(
    const uint8_t *buf, size_t len,
    oigtl_point_element_t *out);


/* Message-level view. The struct-array field is NOT materialized
 * in this struct — use the count/get helpers below to iterate
 * elements out of the wire buffer.
 *
 * Head fields (if any) are available directly. If the message has
 * only the array field, this struct is empty. */
typedef struct oigtl_point {
    /* No head scalars — struct-array is the entire body. This
     * placeholder satisfies C99's prohibition on empty structs
     * without affecting pack/unpack; the real data lives in the
     * nested struct-array elements accessible via the count/get
     * helpers below. */
    char _placeholder;
} oigtl_point_t;

/* Count elements in a wire body. Returns 0 on success with
 * *out_count populated; negative OIGTL_ERR_* on failure (short
 * buffer, misaligned remainder, etc.). */
int oigtl_point_count(const uint8_t *buf, size_t len,
                               size_t *out_count);

/* Random access: copy the idx-th element into *out. Returns 0
 * on success, negative OIGTL_ERR_* otherwise. */
int oigtl_point_get(const uint8_t *buf, size_t len,
                             size_t idx,
                             oigtl_point_element_t *out);

/* Pack an array of elements into a wire body.
 * `msg` is unused for head-less messages — pass NULL.
 * Returns bytes written or a negative OIGTL_ERR_* code. */
int oigtl_point_pack(const oigtl_point_t *msg,
                              const oigtl_point_element_t *elems,
                              size_t count,
                              uint8_t *buf, size_t cap);


#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_POINT_H */
