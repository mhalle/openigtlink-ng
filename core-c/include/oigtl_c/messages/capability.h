/* GENERATED from spec/schemas/capability.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * CAPABILITY — Advertises the list of OpenIGTLink message types that a device accepts. Typically sent in response to a GET_CAPABIL query during connection setup; peers use the advertised list to decide which features to use on the session.
 */
#ifndef OIGTL_C_MESSAGES_CAPABILITY_H
#define OIGTL_C_MESSAGES_CAPABILITY_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Array-of-structs body: see the element struct + helpers below
 * rather than the message-level oigtl_capability_t. */
#define OIGTL_CAPABILITY_ELEMENT_SIZE ((size_t)12)

typedef struct oigtl_capability_element {
    char value[13];
} oigtl_capability_element_t;

/* Pack one element. Returns bytes written (OIGTL_CAPABILITY_ELEMENT_SIZE)
 * or a negative OIGTL_ERR_* code. */
int oigtl_capability_element_pack(
    const oigtl_capability_element_t *elem,
    uint8_t *buf, size_t cap);

/* Unpack one element. Requires cap >= OIGTL_CAPABILITY_ELEMENT_SIZE. */
int oigtl_capability_element_unpack(
    const uint8_t *buf, size_t len,
    oigtl_capability_element_t *out);


/* Message-level view. The struct-array field is NOT materialized
 * in this struct — use the count/get helpers below to iterate
 * elements out of the wire buffer.
 *
 * Head fields (if any) are available directly. If the message has
 * only the array field, this struct is empty. */
typedef struct oigtl_capability {
    /* No head scalars — struct-array is the entire body. This
     * placeholder satisfies C99's prohibition on empty structs
     * without affecting pack/unpack; the real data lives in the
     * nested struct-array elements accessible via the count/get
     * helpers below. */
    char _placeholder;
} oigtl_capability_t;

/* Count elements in a wire body. Returns 0 on success with
 * *out_count populated; negative OIGTL_ERR_* on failure (short
 * buffer, misaligned remainder, etc.). */
int oigtl_capability_count(const uint8_t *buf, size_t len,
                               size_t *out_count);

/* Random access: copy the idx-th element into *out. Returns 0
 * on success, negative OIGTL_ERR_* otherwise. */
int oigtl_capability_get(const uint8_t *buf, size_t len,
                             size_t idx,
                             oigtl_capability_element_t *out);

/* Pack an array of elements into a wire body.
 * `msg` is unused for head-less messages — pass NULL.
 * Returns bytes written or a negative OIGTL_ERR_* code. */
int oigtl_capability_pack(const oigtl_capability_t *msg,
                              const oigtl_capability_element_t *elems,
                              size_t count,
                              uint8_t *buf, size_t cap);


#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_CAPABILITY_H */
