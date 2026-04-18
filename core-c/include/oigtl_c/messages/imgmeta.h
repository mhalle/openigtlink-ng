/* GENERATED from spec/schemas/imgmeta.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * IMGMETA — Advertises the set of IMAGE volumes available on a server. Each element describes one image's name, device suffix, modality, patient identity, acquisition timestamp, spatial dimensions, and pixel type. A client uses IMGMETA to populate an image list without downloading full pixel data first.
 */
#ifndef OIGTL_C_MESSAGES_IMGMETA_H
#define OIGTL_C_MESSAGES_IMGMETA_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Array-of-structs body: see the element struct + helpers below
 * rather than the message-level oigtl_imgmeta_t. */
#define OIGTL_IMGMETA_ELEMENT_SIZE ((size_t)260)

typedef struct oigtl_imgmeta_element {
    char name[65];
    char device_name[21];
    char modality[33];
    char patient_name[65];
    char patient_id[65];
    uint64_t timestamp;
    uint16_t size[3];
    uint8_t scalar_type;
    uint8_t reserved;
} oigtl_imgmeta_element_t;

/* Pack one element. Returns bytes written (OIGTL_IMGMETA_ELEMENT_SIZE)
 * or a negative OIGTL_ERR_* code. */
int oigtl_imgmeta_element_pack(
    const oigtl_imgmeta_element_t *elem,
    uint8_t *buf, size_t cap);

/* Unpack one element. Requires cap >= OIGTL_IMGMETA_ELEMENT_SIZE. */
int oigtl_imgmeta_element_unpack(
    const uint8_t *buf, size_t len,
    oigtl_imgmeta_element_t *out);


/* Message-level view. The struct-array field is NOT materialized
 * in this struct — use the count/get helpers below to iterate
 * elements out of the wire buffer.
 *
 * Head fields (if any) are available directly. If the message has
 * only the array field, this struct is empty. */
typedef struct oigtl_imgmeta {
    /* No head scalars — struct-array is the entire body. This
     * placeholder satisfies C99's prohibition on empty structs
     * without affecting pack/unpack; the real data lives in the
     * nested struct-array elements accessible via the count/get
     * helpers below. */
    char _placeholder;
} oigtl_imgmeta_t;

/* Count elements in a wire body. Returns 0 on success with
 * *out_count populated; negative OIGTL_ERR_* on failure (short
 * buffer, misaligned remainder, etc.). */
int oigtl_imgmeta_count(const uint8_t *buf, size_t len,
                               size_t *out_count);

/* Random access: copy the idx-th element into *out. Returns 0
 * on success, negative OIGTL_ERR_* otherwise. */
int oigtl_imgmeta_get(const uint8_t *buf, size_t len,
                             size_t idx,
                             oigtl_imgmeta_element_t *out);

/* Pack an array of elements into a wire body.
 * `msg` is unused for head-less messages — pass NULL.
 * Returns bytes written or a negative OIGTL_ERR_* code. */
int oigtl_imgmeta_pack(const oigtl_imgmeta_t *msg,
                              const oigtl_imgmeta_element_t *elems,
                              size_t count,
                              uint8_t *buf, size_t cap);


#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_IMGMETA_H */
