/* GENERATED from spec/schemas/rts_polydata.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 */

#include "oigtl_c/messages/rts_polydata.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/errors.h"

int oigtl_rts_polydata_packed_size(const oigtl_rts_polydata_t *msg) {
    if (msg == NULL) return OIGTL_ERR_INVALID_ARG;
    (void)msg;
    return (int)1;
}

int oigtl_rts_polydata_pack(const oigtl_rts_polydata_t *msg,
                              uint8_t *buf, size_t cap) {
    if (msg == NULL || buf == NULL) return OIGTL_ERR_INVALID_ARG;
    const int need_i = oigtl_rts_polydata_packed_size(msg);
    if (need_i < 0) return need_i;
    const size_t need = (size_t)need_i;
    if (cap < need) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* status */
    oigtl_write_be_i8(buf + off, msg->status);
    off += 1;
    (void)off;    /* suppress unused-variable warning for empty bodies */
    return (int)need;
}

int oigtl_rts_polydata_unpack(const uint8_t *buf, size_t len,
                                oigtl_rts_polydata_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;
    if (len < (size_t)1) return OIGTL_ERR_SHORT_BUFFER;
    /* Zero the output so trailing bytes after fixed_string fields
     * and unset view pointers are deterministic. Callers doing a
     * memcmp on two unpack results of the same wire bytes will see
     * equality rather than uninit-memory differences. */
    memset(out, 0, sizeof *out);

    size_t off = 0;
    /* status */
    if (off + 1 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->status = oigtl_read_be_i8(buf + off);
    off += 1;
    (void)off;
    (void)buf;
    (void)len;
    return OIGTL_OK;
}
