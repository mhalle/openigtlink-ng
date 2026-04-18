/* GENERATED from spec/schemas/ext_header.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * EXT_HEADER — The v3 extended header, located at the start of the message body. Carries sizes for the metadata sections and a sender-assigned message ID. This is protocol framing — not a message type. Present only when the outer header's version field is >= 3. A codec uses this to locate the content, metadata index, and metadata body regions within the overall body.
 */
#ifndef OIGTL_C_MESSAGES_EXT_HEADER_H
#define OIGTL_C_MESSAGES_EXT_HEADER_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed body size — every wire-valid EXT_HEADER has this
 * many body bytes. Useful for stack-sizing a pack buffer. */
#define OIGTL_EXT_HEADER_BODY_SIZE ((size_t)12)

typedef struct oigtl_ext_header {
    uint16_t ext_header_size;
    uint16_t metadata_header_size;
    uint32_t metadata_size;
    uint32_t message_id;
} oigtl_ext_header_t;

/* Compute the body size needed to pack `msg`. Returns the byte
 * count on success, or a negative OIGTL_ERR_* code on invalid
 * input. For fixed-body messages this is a constant. */
int oigtl_ext_header_packed_size(const oigtl_ext_header_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code (SHORT_BUFFER /
 * FIELD_RANGE / INVALID_ARG). */
int oigtl_ext_header_pack(const oigtl_ext_header_t *msg,
                              uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — valid only while *buf is valid. Copy via
 * oigtl_copy_string() / oigtl_copy_*_be() before the wire buffer
 * goes away if you need to persist them. */
int oigtl_ext_header_unpack(const uint8_t *buf, size_t len,
                                oigtl_ext_header_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_EXT_HEADER_H */
