/* GENERATED from spec/schemas/sensor.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 */

#include "oigtl_c/messages/sensor.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/errors.h"

int oigtl_sensor_packed_size(const oigtl_sensor_t *msg) {
    if (msg == NULL) return OIGTL_ERR_INVALID_ARG;
    size_t total = (1) + (1) + (8) + (msg->data_bytes);
    /* Clamp to INT_MAX to avoid wraparound in the return; realistic
     * bodies are well under 2 GiB. */
    if (total > (size_t)2147483647) return OIGTL_ERR_FIELD_RANGE;
    return (int)total;
}

int oigtl_sensor_pack(const oigtl_sensor_t *msg,
                              uint8_t *buf, size_t cap) {
    if (msg == NULL || buf == NULL) return OIGTL_ERR_INVALID_ARG;
    const int need_i = oigtl_sensor_packed_size(msg);
    if (need_i < 0) return need_i;
    const size_t need = (size_t)need_i;
    if (cap < need) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* larray */
    oigtl_write_be_u8(buf + off, msg->larray);
    off += 1;
    /* status */
    oigtl_write_be_u8(buf + off, msg->status);
    off += 1;
    /* unit */
    oigtl_write_be_u64(buf + off, msg->unit);
    off += 8;
    /* data */
    if (msg->data_bytes > 0) {
        memcpy(buf + off, msg->data, msg->data_bytes);
        off += msg->data_bytes;
    }
    (void)off;    /* suppress unused-variable warning for empty bodies */
    return (int)need;
}

int oigtl_sensor_unpack(const uint8_t *buf, size_t len,
                                oigtl_sensor_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;

    size_t off = 0;
    /* larray */
    if (off + 1 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->larray = oigtl_read_be_u8(buf + off);
    off += 1;
    /* status */
    if (off + 1 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->status = oigtl_read_be_u8(buf + off);
    off += 1;
    /* unit */
    if (off + 8 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->unit = oigtl_read_be_u64(buf + off);
    off += 8;
    /* data */
    {
        size_t count = (size_t)out->larray;
        size_t bytes = count * 8;
        if (off + bytes > len) return OIGTL_ERR_SHORT_BUFFER;
        out->data = buf + off;
        out->data_bytes = bytes;
        off += bytes;
    }
    (void)off;
    (void)buf;
    (void)len;
    return OIGTL_OK;
}
