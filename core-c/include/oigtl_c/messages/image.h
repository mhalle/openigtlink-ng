/* GENERATED from spec/schemas/image.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * IMAGE — Delivers image pixel data — 2D frames or 3D volumes — together with orientation, origin, scalar type, and optional partial-volume windowing. The body is a 72-byte image header followed by a byte array of pixel data. Supports streaming a full volume in one message or splitting it across multiple messages via the subvolume offset/size window.
 */
#ifndef OIGTL_C_MESSAGES_IMAGE_H
#define OIGTL_C_MESSAGES_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct oigtl_image {
    uint16_t header_version;
    uint8_t num_components;
    uint8_t scalar_type;
    uint8_t endian;
    uint8_t coord;
    uint16_t size[3];
    float matrix[12];
    uint16_t subvol_offset[3];
    uint16_t subvol_size[3];
    /* view: points into wire bytes — see README for lifetime */
    const uint8_t *pixels;
    size_t         pixels_bytes;
} oigtl_image_t;

/* Compute the body size needed to pack `msg`. Returns the byte
 * count on success, or a negative OIGTL_ERR_* code on invalid
 * input. For fixed-body messages this is a constant. */
int oigtl_image_packed_size(const oigtl_image_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code (SHORT_BUFFER /
 * FIELD_RANGE / INVALID_ARG). */
int oigtl_image_pack(const oigtl_image_t *msg,
                              uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — valid only while *buf is valid. Copy via
 * oigtl_copy_string() before the wire buffer goes away if you
 * need to persist them. */
int oigtl_image_unpack(const uint8_t *buf, size_t len,
                                oigtl_image_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_IMAGE_H */
