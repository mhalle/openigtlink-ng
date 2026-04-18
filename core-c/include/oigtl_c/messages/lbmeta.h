/* GENERATED from spec/schemas/lbmeta.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * LBMETA — Advertises the set of label-map regions available as IMAGE messages on a server. Each element describes one label — its human name, the IMAGE device that carries the label data, the integer pixel value that represents the label, a color, the spatial extent, and an optional owner-image reference.
 */
#ifndef OIGTL_C_MESSAGES_LBMETA_H
#define OIGTL_C_MESSAGES_LBMETA_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Array-of-structs body: see the element struct + helpers below
 * rather than the message-level oigtl_lbmeta_t. */
#define OIGTL_LBMETA_ELEMENT_SIZE ((size_t)116)

typedef struct oigtl_lbmeta_element {
    char name[65];
    char device_name[21];
    uint8_t label;
    uint8_t reserved;
    uint8_t rgba[4];
    uint16_t size[3];
    char owner[21];
} oigtl_lbmeta_element_t;

/* Pack one element. Returns bytes written (OIGTL_LBMETA_ELEMENT_SIZE)
 * or a negative OIGTL_ERR_* code. */
int oigtl_lbmeta_element_pack(
    const oigtl_lbmeta_element_t *elem,
    uint8_t *buf, size_t cap);

/* Unpack one element. Requires cap >= OIGTL_LBMETA_ELEMENT_SIZE. */
int oigtl_lbmeta_element_unpack(
    const uint8_t *buf, size_t len,
    oigtl_lbmeta_element_t *out);


/* Message-level view. The struct-array field is NOT materialized
 * in this struct — use the count/get helpers below to iterate
 * elements out of the wire buffer.
 *
 * Head fields (if any) are available directly. If the message has
 * only the array field, this struct is empty. */
typedef struct oigtl_lbmeta {
    /* No head scalars — struct-array is the entire body. This
     * placeholder satisfies C99's prohibition on empty structs
     * without affecting pack/unpack; the real data lives in the
     * nested struct-array elements accessible via the count/get
     * helpers below. */
    char _placeholder;
} oigtl_lbmeta_t;

/* Count elements in a wire body. Returns 0 on success with
 * *out_count populated; negative OIGTL_ERR_* on failure (short
 * buffer, misaligned remainder, etc.). */
int oigtl_lbmeta_count(const uint8_t *buf, size_t len,
                               size_t *out_count);

/* Random access: copy the idx-th element into *out. Returns 0
 * on success, negative OIGTL_ERR_* otherwise. */
int oigtl_lbmeta_get(const uint8_t *buf, size_t len,
                             size_t idx,
                             oigtl_lbmeta_element_t *out);

/* Pack an array of elements into a wire body.
 * `msg` is unused for head-less messages — pass NULL.
 * Returns bytes written or a negative OIGTL_ERR_* code. */
int oigtl_lbmeta_pack(const oigtl_lbmeta_t *msg,
                              const oigtl_lbmeta_element_t *elems,
                              size_t count,
                              uint8_t *buf, size_t cap);


#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_LBMETA_H */
