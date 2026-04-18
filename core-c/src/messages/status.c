/* GENERATED from spec/schemas/status.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 */

#include "oigtl_c/messages/status.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/errors.h"

int oigtl_status_packed_size(const oigtl_status_t *msg) {
    if (msg == NULL) return OIGTL_ERR_INVALID_ARG;
    size_t total = (2) + (8) + (20) + (msg->status_message_len + 1);
    /* Clamp to INT_MAX to avoid wraparound in the return; realistic
     * bodies are well under 2 GiB. */
    if (total > (size_t)2147483647) return OIGTL_ERR_FIELD_RANGE;
    return (int)total;
}

int oigtl_status_pack(const oigtl_status_t *msg,
                              uint8_t *buf, size_t cap) {
    if (msg == NULL || buf == NULL) return OIGTL_ERR_INVALID_ARG;
    const int need_i = oigtl_status_packed_size(msg);
    if (need_i < 0) return need_i;
    const size_t need = (size_t)need_i;
    if (cap < need) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* code */
    oigtl_write_be_u16(buf + off, msg->code);
    off += 2;
    /* subcode */
    oigtl_write_be_i64(buf + off, msg->subcode);
    off += 8;
    /* error_name */
    {
        size_t n = strlen(msg->error_name);
        if (n > 20) return OIGTL_ERR_FIELD_RANGE;
        memcpy(buf + off, msg->error_name, n);
        if (n < 20) memset(buf + off + n, 0, 20 - n);
        off += 20;
    }
    /* status_message */
    if (msg->status_message_len > 0) {
        memcpy(buf + off, msg->status_message, msg->status_message_len);
        off += msg->status_message_len;
    }
    buf[off] = 0;
    off += 1;
    (void)off;    /* suppress unused-variable warning for empty bodies */
    return (int)need;
}

int oigtl_status_unpack(const uint8_t *buf, size_t len,
                                oigtl_status_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;

    size_t off = 0;
    /* code */
    if (off + 2 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->code = oigtl_read_be_u16(buf + off);
    off += 2;
    /* subcode */
    if (off + 8 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->subcode = oigtl_read_be_i64(buf + off);
    off += 8;
    /* error_name */
    if (off + 20 > len) return OIGTL_ERR_SHORT_BUFFER;
    {
        int n = oigtl_null_padded_length(buf + off, 20);
        if (n < 0) return n;
        memcpy(out->error_name, buf + off, (size_t)n);
        out->error_name[n] = '\0';
        off += 20;
    }
    /* status_message */
    {
        size_t end = len;
        if (end <= off) return OIGTL_ERR_MALFORMED;
        if (buf[end - 1] != 0) return OIGTL_ERR_MALFORMED;
        --end;
        out->status_message = (const char *)(buf + off);
        out->status_message_len = end - off;
        off = len;
    }
    (void)off;
    (void)buf;
    (void)len;
    return OIGTL_OK;
}
