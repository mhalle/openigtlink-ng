/* GENERATED from spec/schemas/rts_command.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 */

#include "oigtl_c/messages/rts_command.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/errors.h"

int oigtl_rts_command_packed_size(const oigtl_rts_command_t *msg) {
    if (msg == NULL) return OIGTL_ERR_INVALID_ARG;
    size_t total = (4) + (128) + (2) + (4) + (msg->command_bytes);
    /* Clamp to INT_MAX to avoid wraparound in the return; realistic
     * bodies are well under 2 GiB. */
    if (total > (size_t)2147483647) return OIGTL_ERR_FIELD_RANGE;
    return (int)total;
}

int oigtl_rts_command_pack(const oigtl_rts_command_t *msg,
                              uint8_t *buf, size_t cap) {
    if (msg == NULL || buf == NULL) return OIGTL_ERR_INVALID_ARG;
    const int need_i = oigtl_rts_command_packed_size(msg);
    if (need_i < 0) return need_i;
    const size_t need = (size_t)need_i;
    if (cap < need) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* command_id */
    oigtl_write_be_u32(buf + off, msg->command_id);
    off += 4;
    /* command_name */
    {
        size_t n = strlen(msg->command_name);
        if (n > 128) return OIGTL_ERR_FIELD_RANGE;
        memcpy(buf + off, msg->command_name, n);
        if (n < 128) memset(buf + off + n, 0, 128 - n);
        off += 128;
    }
    /* encoding */
    oigtl_write_be_u16(buf + off, msg->encoding);
    off += 2;
    /* length */
    oigtl_write_be_u32(buf + off, msg->length);
    off += 4;
    /* command */
    if (msg->command_bytes > 0) {
        memcpy(buf + off, msg->command, msg->command_bytes);
        off += msg->command_bytes;
    }
    (void)off;    /* suppress unused-variable warning for empty bodies */
    return (int)need;
}

int oigtl_rts_command_unpack(const uint8_t *buf, size_t len,
                                oigtl_rts_command_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;
    /* Zero the output so trailing bytes after fixed_string fields
     * and unset view pointers are deterministic. Callers doing a
     * memcmp on two unpack results of the same wire bytes will see
     * equality rather than uninit-memory differences. */
    memset(out, 0, sizeof *out);

    size_t off = 0;
    /* command_id */
    if (off + 4 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->command_id = oigtl_read_be_u32(buf + off);
    off += 4;
    /* command_name */
    if (off + 128 > len) return OIGTL_ERR_SHORT_BUFFER;
    {
        int n = oigtl_null_padded_length(buf + off, 128);
        if (n < 0) return n;
        memcpy(out->command_name, buf + off, (size_t)n);
        out->command_name[n] = '\0';
        off += 128;
    }
    /* encoding */
    if (off + 2 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->encoding = oigtl_read_be_u16(buf + off);
    off += 2;
    /* length */
    if (off + 4 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->length = oigtl_read_be_u32(buf + off);
    off += 4;
    /* command */
    {
        size_t count = (size_t)out->length;
        size_t bytes = count * 1;
        if (off + bytes > len) return OIGTL_ERR_SHORT_BUFFER;
        out->command = buf + off;
        out->command_bytes = bytes;
        off += bytes;
    }
    (void)off;
    (void)buf;
    (void)len;
    return OIGTL_OK;
}
