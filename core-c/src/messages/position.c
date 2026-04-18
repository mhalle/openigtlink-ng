/* GENERATED from spec/schemas/position.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 */

#include "oigtl_c/messages/position.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/errors.h"

int oigtl_position_packed_size(const oigtl_position_t *msg) {
    if (msg == NULL) return OIGTL_ERR_INVALID_ARG;
    size_t total = (12) + (msg->quaternion_bytes);
    /* Clamp to INT_MAX to avoid wraparound in the return; realistic
     * bodies are well under 2 GiB. */
    if (total > (size_t)2147483647) return OIGTL_ERR_FIELD_RANGE;
    return (int)total;
}

int oigtl_position_pack(const oigtl_position_t *msg,
                              uint8_t *buf, size_t cap) {
    if (msg == NULL || buf == NULL) return OIGTL_ERR_INVALID_ARG;
    const int need_i = oigtl_position_packed_size(msg);
    if (need_i < 0) return need_i;
    const size_t need = (size_t)need_i;
    if (cap < need) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* position */
    for (size_t i = 0; i < 3; ++i) {
        oigtl_write_be_f32(buf + off + i * 4, msg->position[i]);
    }
    off += 12;
    /* quaternion */
    if (msg->quaternion_bytes > 0) {
        memcpy(buf + off, msg->quaternion, msg->quaternion_bytes);
        off += msg->quaternion_bytes;
    }
    (void)off;    /* suppress unused-variable warning for empty bodies */
    return (int)need;
}

int oigtl_position_unpack(const uint8_t *buf, size_t len,
                                oigtl_position_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;
    /* Zero the output so trailing bytes after fixed_string fields
     * and unset view pointers are deterministic. Callers doing a
     * memcmp on two unpack results of the same wire bytes will see
     * equality rather than uninit-memory differences. */
    memset(out, 0, sizeof *out);
    {
        static const size_t k_allowed[] = {
            (size_t)12,
            (size_t)24,
            (size_t)28,
        };
        int ok = 0;
        for (size_t i = 0; i < sizeof k_allowed / sizeof k_allowed[0]; ++i) {
            if (len == k_allowed[i]) { ok = 1; break; }
        }
        if (!ok) return OIGTL_ERR_MALFORMED;
    }

    size_t off = 0;
    /* position */
    if (off + 12 > len) return OIGTL_ERR_SHORT_BUFFER;
    for (size_t i = 0; i < 3; ++i) {
        out->position[i] = oigtl_read_be_f32(buf + off + i * 4);
    }
    off += 12;
    /* quaternion */
    {
        size_t bytes = len - off;
        if (bytes % 4 != 0) return OIGTL_ERR_MALFORMED;
        out->quaternion = buf + off;
        out->quaternion_bytes = bytes;
        off += bytes;
    }
    (void)off;
    (void)buf;
    (void)len;
    return OIGTL_OK;
}
