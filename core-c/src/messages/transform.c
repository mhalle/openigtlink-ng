/* GENERATED from spec/schemas/transform.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 */

#include "oigtl_c/messages/transform.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/errors.h"

int oigtl_transform_packed_size(const oigtl_transform_t *msg) {
    if (msg == NULL) return OIGTL_ERR_INVALID_ARG;
    (void)msg;
    return (int)48;
}

int oigtl_transform_pack(const oigtl_transform_t *msg,
                              uint8_t *buf, size_t cap) {
    if (msg == NULL || buf == NULL) return OIGTL_ERR_INVALID_ARG;
    const int need_i = oigtl_transform_packed_size(msg);
    if (need_i < 0) return need_i;
    const size_t need = (size_t)need_i;
    if (cap < need) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* matrix */
    for (size_t i = 0; i < 12; ++i) {
        oigtl_write_be_f32(buf + off + i * 4, msg->matrix[i]);
    }
    off += 48;
    (void)off;    /* suppress unused-variable warning for empty bodies */
    return (int)need;
}

int oigtl_transform_unpack(const uint8_t *buf, size_t len,
                                oigtl_transform_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;
    if (len < (size_t)48) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* matrix */
    if (off + 48 > len) return OIGTL_ERR_SHORT_BUFFER;
    for (size_t i = 0; i < 12; ++i) {
        out->matrix[i] = oigtl_read_be_f32(buf + off + i * 4);
    }
    off += 48;
    (void)off;
    (void)buf;
    (void)len;
    return OIGTL_OK;
}
