/* GENERATED from spec/schemas/colortable.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 */

#include "oigtl_c/messages/colortable.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/errors.h"

int oigtl_colortable_packed_size(const oigtl_colortable_t *msg) {
    if (msg == NULL) return OIGTL_ERR_INVALID_ARG;
    size_t total = (1) + (1) + (msg->table_bytes);
    /* Clamp to INT_MAX to avoid wraparound in the return; realistic
     * bodies are well under 2 GiB. */
    if (total > (size_t)2147483647) return OIGTL_ERR_FIELD_RANGE;
    return (int)total;
}

int oigtl_colortable_pack(const oigtl_colortable_t *msg,
                              uint8_t *buf, size_t cap) {
    if (msg == NULL || buf == NULL) return OIGTL_ERR_INVALID_ARG;
    const int need_i = oigtl_colortable_packed_size(msg);
    if (need_i < 0) return need_i;
    const size_t need = (size_t)need_i;
    if (cap < need) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* index_type */
    oigtl_write_be_i8(buf + off, msg->index_type);
    off += 1;
    /* map_type */
    oigtl_write_be_i8(buf + off, msg->map_type);
    off += 1;
    /* table */
    if (msg->table_bytes > 0) {
        memcpy(buf + off, msg->table, msg->table_bytes);
        off += msg->table_bytes;
    }
    (void)off;    /* suppress unused-variable warning for empty bodies */
    return (int)need;
}

int oigtl_colortable_unpack(const uint8_t *buf, size_t len,
                                oigtl_colortable_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;
    /* Zero the output so trailing bytes after fixed_string fields
     * and unset view pointers are deterministic. Callers doing a
     * memcmp on two unpack results of the same wire bytes will see
     * equality rather than uninit-memory differences. */
    memset(out, 0, sizeof *out);

    size_t off = 0;
    /* index_type */
    if (off + 1 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->index_type = oigtl_read_be_i8(buf + off);
    off += 1;
    /* map_type */
    if (off + 1 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->map_type = oigtl_read_be_i8(buf + off);
    off += 1;
    /* table */
    {
        size_t bytes = len - off;
        if (bytes % 1 != 0) return OIGTL_ERR_MALFORMED;
        out->table = buf + off;
        out->table_bytes = bytes;
        off += bytes;
    }
    (void)off;
    (void)buf;
    (void)len;
    return OIGTL_OK;
}
