/* GENERATED from spec/schemas/stt_tdata.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 */

#include "oigtl_c/messages/stt_tdata.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/errors.h"

int oigtl_stt_tdata_packed_size(const oigtl_stt_tdata_t *msg) {
    if (msg == NULL) return OIGTL_ERR_INVALID_ARG;
    (void)msg;
    return (int)36;
}

int oigtl_stt_tdata_pack(const oigtl_stt_tdata_t *msg,
                              uint8_t *buf, size_t cap) {
    if (msg == NULL || buf == NULL) return OIGTL_ERR_INVALID_ARG;
    const int need_i = oigtl_stt_tdata_packed_size(msg);
    if (need_i < 0) return need_i;
    const size_t need = (size_t)need_i;
    if (cap < need) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* resolution */
    oigtl_write_be_i32(buf + off, msg->resolution);
    off += 4;
    /* coord_name */
    {
        size_t n = strlen(msg->coord_name);
        if (n > 32) return OIGTL_ERR_FIELD_RANGE;
        memcpy(buf + off, msg->coord_name, n);
        if (n < 32) memset(buf + off + n, 0, 32 - n);
        off += 32;
    }
    (void)off;    /* suppress unused-variable warning for empty bodies */
    return (int)need;
}

int oigtl_stt_tdata_unpack(const uint8_t *buf, size_t len,
                                oigtl_stt_tdata_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;
    if (len < (size_t)36) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* resolution */
    if (off + 4 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->resolution = oigtl_read_be_i32(buf + off);
    off += 4;
    /* coord_name */
    if (off + 32 > len) return OIGTL_ERR_SHORT_BUFFER;
    {
        int n = oigtl_null_padded_length(buf + off, 32);
        if (n < 0) return n;
        memcpy(out->coord_name, buf + off, (size_t)n);
        out->coord_name[n] = '\0';
        off += 32;
    }
    (void)off;
    (void)buf;
    (void)len;
    return OIGTL_OK;
}
