/* GENERATED from spec/schemas/ndarray.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 */

#include "oigtl_c/messages/ndarray.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/errors.h"

int oigtl_ndarray_packed_size(const oigtl_ndarray_t *msg) {
    if (msg == NULL) return OIGTL_ERR_INVALID_ARG;
    size_t total = (1) + (1) + (msg->size_bytes) + (msg->data_bytes);
    /* Clamp to INT_MAX to avoid wraparound in the return; realistic
     * bodies are well under 2 GiB. */
    if (total > (size_t)2147483647) return OIGTL_ERR_FIELD_RANGE;
    return (int)total;
}

int oigtl_ndarray_pack(const oigtl_ndarray_t *msg,
                              uint8_t *buf, size_t cap) {
    if (msg == NULL || buf == NULL) return OIGTL_ERR_INVALID_ARG;
    const int need_i = oigtl_ndarray_packed_size(msg);
    if (need_i < 0) return need_i;
    const size_t need = (size_t)need_i;
    if (cap < need) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* scalar_type */
    oigtl_write_be_u8(buf + off, msg->scalar_type);
    off += 1;
    /* dim */
    oigtl_write_be_u8(buf + off, msg->dim);
    off += 1;
    /* size */
    if (msg->size_bytes > 0) {
        memcpy(buf + off, msg->size, msg->size_bytes);
        off += msg->size_bytes;
    }
    /* data */
    if (msg->data_bytes > 0) {
        memcpy(buf + off, msg->data, msg->data_bytes);
        off += msg->data_bytes;
    }
    (void)off;    /* suppress unused-variable warning for empty bodies */
    return (int)need;
}

int oigtl_ndarray_unpack(const uint8_t *buf, size_t len,
                                oigtl_ndarray_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;

    size_t off = 0;
    /* scalar_type */
    if (off + 1 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->scalar_type = oigtl_read_be_u8(buf + off);
    off += 1;
    /* dim */
    if (off + 1 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->dim = oigtl_read_be_u8(buf + off);
    off += 1;
    /* size */
    {
        size_t count = (size_t)out->dim;
        size_t bytes = count * 2;
        if (off + bytes > len) return OIGTL_ERR_SHORT_BUFFER;
        out->size = buf + off;
        out->size_bytes = bytes;
        off += bytes;
    }
    /* data */
    {
        size_t bytes = len - off;
        if (bytes % 1 != 0) return OIGTL_ERR_MALFORMED;
        out->data = buf + off;
        out->data_bytes = bytes;
        off += bytes;
    }
    (void)off;
    (void)buf;
    (void)len;
    return OIGTL_OK;
}
