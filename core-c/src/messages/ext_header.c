/* GENERATED from spec/schemas/ext_header.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 */

#include "oigtl_c/messages/ext_header.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/errors.h"

int oigtl_ext_header_packed_size(const oigtl_ext_header_t *msg) {
    if (msg == NULL) return OIGTL_ERR_INVALID_ARG;
    (void)msg;
    return (int)12;
}

int oigtl_ext_header_pack(const oigtl_ext_header_t *msg,
                              uint8_t *buf, size_t cap) {
    if (msg == NULL || buf == NULL) return OIGTL_ERR_INVALID_ARG;
    const int need_i = oigtl_ext_header_packed_size(msg);
    if (need_i < 0) return need_i;
    const size_t need = (size_t)need_i;
    if (cap < need) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* ext_header_size */
    oigtl_write_be_u16(buf + off, msg->ext_header_size);
    off += 2;
    /* metadata_header_size */
    oigtl_write_be_u16(buf + off, msg->metadata_header_size);
    off += 2;
    /* metadata_size */
    oigtl_write_be_u32(buf + off, msg->metadata_size);
    off += 4;
    /* message_id */
    oigtl_write_be_u32(buf + off, msg->message_id);
    off += 4;
    (void)off;    /* suppress unused-variable warning for empty bodies */
    return (int)need;
}

int oigtl_ext_header_unpack(const uint8_t *buf, size_t len,
                                oigtl_ext_header_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;
    if (len < (size_t)12) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* ext_header_size */
    if (off + 2 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->ext_header_size = oigtl_read_be_u16(buf + off);
    off += 2;
    /* metadata_header_size */
    if (off + 2 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->metadata_header_size = oigtl_read_be_u16(buf + off);
    off += 2;
    /* metadata_size */
    if (off + 4 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->metadata_size = oigtl_read_be_u32(buf + off);
    off += 4;
    /* message_id */
    if (off + 4 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->message_id = oigtl_read_be_u32(buf + off);
    off += 4;
    (void)off;
    (void)buf;
    (void)len;
    return OIGTL_OK;
}
