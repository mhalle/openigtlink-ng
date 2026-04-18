/* GENERATED from spec/schemas/stp_position.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 */

#include "oigtl_c/messages/stp_position.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/errors.h"

int oigtl_stp_position_packed_size(const oigtl_stp_position_t *msg) {
    if (msg == NULL) return OIGTL_ERR_INVALID_ARG;
    (void)msg;
    return (int)0;
}

int oigtl_stp_position_pack(const oigtl_stp_position_t *msg,
                              uint8_t *buf, size_t cap) {
    if (msg == NULL || buf == NULL) return OIGTL_ERR_INVALID_ARG;
    const int need_i = oigtl_stp_position_packed_size(msg);
    if (need_i < 0) return need_i;
    const size_t need = (size_t)need_i;
    if (cap < need) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    (void)off;    /* suppress unused-variable warning for empty bodies */
    return (int)need;
}

int oigtl_stp_position_unpack(const uint8_t *buf, size_t len,
                                oigtl_stp_position_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;

    size_t off = 0;
    (void)off;
    (void)buf;
    (void)len;
    return OIGTL_OK;
}
