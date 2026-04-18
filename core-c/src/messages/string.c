/* GENERATED from spec/schemas/string.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 */

#include "oigtl_c/messages/string.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/errors.h"

int oigtl_string_packed_size(const oigtl_string_t *msg) {
    if (msg == NULL) return OIGTL_ERR_INVALID_ARG;
    size_t total = (2) + (2 + msg->value_len);
    /* Clamp to INT_MAX to avoid wraparound in the return; realistic
     * bodies are well under 2 GiB. */
    if (total > (size_t)2147483647) return OIGTL_ERR_FIELD_RANGE;
    return (int)total;
}

int oigtl_string_pack(const oigtl_string_t *msg,
                              uint8_t *buf, size_t cap) {
    if (msg == NULL || buf == NULL) return OIGTL_ERR_INVALID_ARG;
    const int need_i = oigtl_string_packed_size(msg);
    if (need_i < 0) return need_i;
    const size_t need = (size_t)need_i;
    if (cap < need) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* encoding */
    oigtl_write_be_u16(buf + off, msg->encoding);
    off += 2;
    /* value */
    if (msg->value_len > 0xFFFFu) return OIGTL_ERR_FIELD_RANGE;
    oigtl_write_be_u16(buf + off, (uint16_t)msg->value_len);
    off += 2;
    if (msg->value_len > 0) {
        memcpy(buf + off, msg->value, msg->value_len);
        off += msg->value_len;
    }
    (void)off;    /* suppress unused-variable warning for empty bodies */
    return (int)need;
}

int oigtl_string_unpack(const uint8_t *buf, size_t len,
                                oigtl_string_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;
    /* Zero the output so trailing bytes after fixed_string fields
     * and unset view pointers are deterministic. Callers doing a
     * memcmp on two unpack results of the same wire bytes will see
     * equality rather than uninit-memory differences. */
    memset(out, 0, sizeof *out);

    size_t off = 0;
    /* encoding */
    if (off + 2 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->encoding = oigtl_read_be_u16(buf + off);
    off += 2;
    /* value */
    if (off + 2 > len) return OIGTL_ERR_SHORT_BUFFER;
    {
        size_t n = (size_t)oigtl_read_be_u16(buf + off);
        off += 2;
        if (off + n > len) return OIGTL_ERR_SHORT_BUFFER;
        out->value = (const char *)(buf + off);
        out->value_len = n;
        off += n;
    }
    (void)off;
    (void)buf;
    (void)len;
    return OIGTL_OK;
}
