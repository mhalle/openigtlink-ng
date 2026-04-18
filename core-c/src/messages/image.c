/* GENERATED from spec/schemas/image.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 */

#include "oigtl_c/messages/image.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/errors.h"

int oigtl_image_packed_size(const oigtl_image_t *msg) {
    if (msg == NULL) return OIGTL_ERR_INVALID_ARG;
    size_t total = (2) + (1) + (1) + (1) + (1) + (6) + (48) + (6) + (6) + (msg->pixels_bytes);
    /* Clamp to INT_MAX to avoid wraparound in the return; realistic
     * bodies are well under 2 GiB. */
    if (total > (size_t)2147483647) return OIGTL_ERR_FIELD_RANGE;
    return (int)total;
}

int oigtl_image_pack(const oigtl_image_t *msg,
                              uint8_t *buf, size_t cap) {
    if (msg == NULL || buf == NULL) return OIGTL_ERR_INVALID_ARG;
    const int need_i = oigtl_image_packed_size(msg);
    if (need_i < 0) return need_i;
    const size_t need = (size_t)need_i;
    if (cap < need) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* header_version */
    oigtl_write_be_u16(buf + off, msg->header_version);
    off += 2;
    /* num_components */
    oigtl_write_be_u8(buf + off, msg->num_components);
    off += 1;
    /* scalar_type */
    oigtl_write_be_u8(buf + off, msg->scalar_type);
    off += 1;
    /* endian */
    oigtl_write_be_u8(buf + off, msg->endian);
    off += 1;
    /* coord */
    oigtl_write_be_u8(buf + off, msg->coord);
    off += 1;
    /* size */
    for (size_t i = 0; i < 3; ++i) {
        oigtl_write_be_u16(buf + off + i * 2, msg->size[i]);
    }
    off += 6;
    /* matrix */
    for (size_t i = 0; i < 12; ++i) {
        oigtl_write_be_f32(buf + off + i * 4, msg->matrix[i]);
    }
    off += 48;
    /* subvol_offset */
    for (size_t i = 0; i < 3; ++i) {
        oigtl_write_be_u16(buf + off + i * 2, msg->subvol_offset[i]);
    }
    off += 6;
    /* subvol_size */
    for (size_t i = 0; i < 3; ++i) {
        oigtl_write_be_u16(buf + off + i * 2, msg->subvol_size[i]);
    }
    off += 6;
    /* pixels */
    if (msg->pixels_bytes > 0) {
        memcpy(buf + off, msg->pixels, msg->pixels_bytes);
        off += msg->pixels_bytes;
    }
    (void)off;    /* suppress unused-variable warning for empty bodies */
    return (int)need;
}

int oigtl_image_unpack(const uint8_t *buf, size_t len,
                                oigtl_image_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;
    /* Zero the output so trailing bytes after fixed_string fields
     * and unset view pointers are deterministic. Callers doing a
     * memcmp on two unpack results of the same wire bytes will see
     * equality rather than uninit-memory differences. */
    memset(out, 0, sizeof *out);

    size_t off = 0;
    /* header_version */
    if (off + 2 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->header_version = oigtl_read_be_u16(buf + off);
    off += 2;
    /* num_components */
    if (off + 1 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->num_components = oigtl_read_be_u8(buf + off);
    off += 1;
    /* scalar_type */
    if (off + 1 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->scalar_type = oigtl_read_be_u8(buf + off);
    off += 1;
    /* endian */
    if (off + 1 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->endian = oigtl_read_be_u8(buf + off);
    off += 1;
    /* coord */
    if (off + 1 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->coord = oigtl_read_be_u8(buf + off);
    off += 1;
    /* size */
    if (off + 6 > len) return OIGTL_ERR_SHORT_BUFFER;
    for (size_t i = 0; i < 3; ++i) {
        out->size[i] = oigtl_read_be_u16(buf + off + i * 2);
    }
    off += 6;
    /* matrix */
    if (off + 48 > len) return OIGTL_ERR_SHORT_BUFFER;
    for (size_t i = 0; i < 12; ++i) {
        out->matrix[i] = oigtl_read_be_f32(buf + off + i * 4);
    }
    off += 48;
    /* subvol_offset */
    if (off + 6 > len) return OIGTL_ERR_SHORT_BUFFER;
    for (size_t i = 0; i < 3; ++i) {
        out->subvol_offset[i] = oigtl_read_be_u16(buf + off + i * 2);
    }
    off += 6;
    /* subvol_size */
    if (off + 6 > len) return OIGTL_ERR_SHORT_BUFFER;
    for (size_t i = 0; i < 3; ++i) {
        out->subvol_size[i] = oigtl_read_be_u16(buf + off + i * 2);
    }
    off += 6;
    /* pixels */
    {
        size_t bytes = len - off;
        if (bytes % 1 != 0) return OIGTL_ERR_MALFORMED;
        out->pixels = buf + off;
        out->pixels_bytes = bytes;
        off += bytes;
    }
    (void)off;
    (void)buf;
    (void)len;
    return OIGTL_OK;
}
