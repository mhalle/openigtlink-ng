/* GENERATED from spec/schemas/video.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 *
 * VIDEO — Compressed video frame with in-band orientation. The body is a 76-byte frame header followed by the codec-compressed frame payload. Structurally similar to IMAGE but with a FourCC codec identifier instead of scalar_type/num_components, and no meaningful byte-count relationship between subvol_size and the compressed frame data (the frame payload size is determined by the codec, not by the pixel dimensions).
 */
#ifndef OIGTL_C_MESSAGES_VIDEO_H
#define OIGTL_C_MESSAGES_VIDEO_H

#include <stddef.h>
#include <stdint.h>

#include "oigtl_c/errors.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct oigtl_video {
    uint16_t header_version;
    uint8_t endian;
    char codec[5];
    uint16_t frame_type;
    uint8_t coord;
    uint16_t size[3];
    float matrix[12];
    uint16_t subvol_offset[3];
    uint16_t subvol_size[3];
    /* view: points into wire bytes — see README for lifetime */
    const uint8_t *frame_data;
    size_t         frame_data_bytes;
} oigtl_video_t;

/* Compute the body size needed to pack `msg`. Returns the byte
 * count on success, or a negative OIGTL_ERR_* code on invalid
 * input. For fixed-body messages this is a constant. */
int oigtl_video_packed_size(const oigtl_video_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code (SHORT_BUFFER /
 * FIELD_RANGE / INVALID_ARG). */
int oigtl_video_pack(const oigtl_video_t *msg,
                              uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — valid only while *buf is valid. Copy via
 * oigtl_copy_string() before the wire buffer goes away if you
 * need to persist them. */
int oigtl_video_unpack(const uint8_t *buf, size_t len,
                                oigtl_video_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_MESSAGES_VIDEO_H */
