/* GENERATED from spec/schemas/header.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 */

#include "oigtl_c/messages/header.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/errors.h"

int oigtl_header_packed_size(const oigtl_header_t *msg) {
    if (msg == NULL) return OIGTL_ERR_INVALID_ARG;
    (void)msg;
    return (int)58;
}

int oigtl_header_pack(const oigtl_header_t *msg,
                              uint8_t *buf, size_t cap) {
    if (msg == NULL || buf == NULL) return OIGTL_ERR_INVALID_ARG;
    const int need_i = oigtl_header_packed_size(msg);
    if (need_i < 0) return need_i;
    const size_t need = (size_t)need_i;
    if (cap < need) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* version */
    oigtl_write_be_u16(buf + off, msg->version);
    off += 2;
    /* type */
    {
        size_t n = strlen(msg->type);
        if (n > 12) return OIGTL_ERR_FIELD_RANGE;
        memcpy(buf + off, msg->type, n);
        if (n < 12) memset(buf + off + n, 0, 12 - n);
        off += 12;
    }
    /* device_name */
    {
        size_t n = strlen(msg->device_name);
        if (n > 20) return OIGTL_ERR_FIELD_RANGE;
        memcpy(buf + off, msg->device_name, n);
        if (n < 20) memset(buf + off + n, 0, 20 - n);
        off += 20;
    }
    /* timestamp */
    oigtl_write_be_u64(buf + off, msg->timestamp);
    off += 8;
    /* body_size */
    oigtl_write_be_u64(buf + off, msg->body_size);
    off += 8;
    /* crc */
    oigtl_write_be_u64(buf + off, msg->crc);
    off += 8;
    (void)off;    /* suppress unused-variable warning for empty bodies */
    return (int)need;
}

int oigtl_header_unpack(const uint8_t *buf, size_t len,
                                oigtl_header_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;
    if (len < (size_t)58) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* version */
    if (off + 2 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->version = oigtl_read_be_u16(buf + off);
    off += 2;
    /* type */
    if (off + 12 > len) return OIGTL_ERR_SHORT_BUFFER;
    {
        int n = oigtl_null_padded_length(buf + off, 12);
        if (n < 0) return n;
        memcpy(out->type, buf + off, (size_t)n);
        out->type[n] = '\0';
        off += 12;
    }
    /* device_name */
    if (off + 20 > len) return OIGTL_ERR_SHORT_BUFFER;
    {
        int n = oigtl_null_padded_length(buf + off, 20);
        if (n < 0) return n;
        memcpy(out->device_name, buf + off, (size_t)n);
        out->device_name[n] = '\0';
        off += 20;
    }
    /* timestamp */
    if (off + 8 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->timestamp = oigtl_read_be_u64(buf + off);
    off += 8;
    /* body_size */
    if (off + 8 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->body_size = oigtl_read_be_u64(buf + off);
    off += 8;
    /* crc */
    if (off + 8 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->crc = oigtl_read_be_u64(buf + off);
    off += 8;
    (void)off;
    (void)buf;
    (void)len;
    return OIGTL_OK;
}
