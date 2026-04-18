/* GENERATED from spec/schemas/stt_video.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 */

#include "oigtl_c/messages/stt_video.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/errors.h"

int oigtl_stt_video_packed_size(const oigtl_stt_video_t *msg) {
    if (msg == NULL) return OIGTL_ERR_INVALID_ARG;
    (void)msg;
    return (int)8;
}

int oigtl_stt_video_pack(const oigtl_stt_video_t *msg,
                              uint8_t *buf, size_t cap) {
    if (msg == NULL || buf == NULL) return OIGTL_ERR_INVALID_ARG;
    const int need_i = oigtl_stt_video_packed_size(msg);
    if (need_i < 0) return need_i;
    const size_t need = (size_t)need_i;
    if (cap < need) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* codec */
    {
        size_t n = strlen(msg->codec);
        if (n != 4) return OIGTL_ERR_FIELD_RANGE;
        memcpy(buf + off, msg->codec, 4);
        off += 4;
    }
    /* time_interval */
    oigtl_write_be_u32(buf + off, msg->time_interval);
    off += 4;
    (void)off;    /* suppress unused-variable warning for empty bodies */
    return (int)need;
}

int oigtl_stt_video_unpack(const uint8_t *buf, size_t len,
                                oigtl_stt_video_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;
    if (len < (size_t)8) return OIGTL_ERR_SHORT_BUFFER;

    size_t off = 0;
    /* codec */
    if (off + 4 > len) return OIGTL_ERR_SHORT_BUFFER;
    memcpy(out->codec, buf + off, 4);
    out->codec[4] = '\0';
    off += 4;
    /* time_interval */
    if (off + 4 > len) return OIGTL_ERR_SHORT_BUFFER;
    out->time_interval = oigtl_read_be_u32(buf + off);
    off += 4;
    (void)off;
    (void)buf;
    (void)len;
    return OIGTL_OK;
}
