/* GENERATED from spec/schemas/qtdata.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 */

#include "oigtl_c/messages/qtdata.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/errors.h"

/* ------------------------------------------------------------------ */
/* Element pack/unpack                                                  */
/* ------------------------------------------------------------------ */

int oigtl_qtdata_element_pack(
    const oigtl_qtdata_element_t *elem,
    uint8_t *buf, size_t cap) {
    if (elem == NULL || buf == NULL) return OIGTL_ERR_INVALID_ARG;
    if (cap < OIGTL_QTDATA_ELEMENT_SIZE)
        return OIGTL_ERR_SHORT_BUFFER;

    size_t eoff = 0;
    {
        size_t n = strlen(elem->name);
        if (n > 20) return OIGTL_ERR_FIELD_RANGE;
        memcpy(buf + eoff, elem->name, n);
        if (n < 20) memset(buf + eoff + n, 0, 20 - n);
        eoff += 20;
    }
    oigtl_write_be_u8(buf + eoff, elem->type);
    eoff += 1;
    oigtl_write_be_u8(buf + eoff, elem->reserved);
    eoff += 1;
    for (size_t i = 0; i < 3; ++i) {
        oigtl_write_be_f32(buf + eoff + i * 4, elem->position[i]);
    }
    eoff += 12;
    for (size_t i = 0; i < 4; ++i) {
        oigtl_write_be_f32(buf + eoff + i * 4, elem->quaternion[i]);
    }
    eoff += 16;
    (void)eoff;
    return (int)OIGTL_QTDATA_ELEMENT_SIZE;
}

int oigtl_qtdata_element_unpack(
    const uint8_t *buf, size_t len,
    oigtl_qtdata_element_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;
    if (len < OIGTL_QTDATA_ELEMENT_SIZE)
        return OIGTL_ERR_SHORT_BUFFER;

    size_t eoff = 0;
    {
        int n = oigtl_null_padded_length(buf + eoff, 20);
        if (n < 0) return n;
        memcpy(out->name, buf + eoff, (size_t)n);
        out->name[n] = '\0';
        eoff += 20;
    }
    out->type = oigtl_read_be_u8(buf + eoff);
    eoff += 1;
    out->reserved = oigtl_read_be_u8(buf + eoff);
    eoff += 1;
    for (size_t i = 0; i < 3; ++i) {
        out->position[i] = oigtl_read_be_f32(buf + eoff + i * 4);
    }
    eoff += 12;
    for (size_t i = 0; i < 4; ++i) {
        out->quaternion[i] = oigtl_read_be_f32(buf + eoff + i * 4);
    }
    eoff += 16;
    (void)eoff;
    (void)buf;
    (void)len;
    return OIGTL_OK;
}

/* ------------------------------------------------------------------ */
/* Message-level helpers                                                */
/* ------------------------------------------------------------------ */

/* Head-scalar bytes: primitives / fixed fields preceding the array. */
/* No head fields; head helpers are no-ops. */
#define _oigtl_qtdata_HEAD_SIZE 0

int oigtl_qtdata_count(const uint8_t *buf, size_t len,
                               size_t *out_count) {
    if (buf == NULL || out_count == NULL) return OIGTL_ERR_INVALID_ARG;
    size_t off = 0;
    (void)buf;
    if (off > len) return OIGTL_ERR_SHORT_BUFFER;
    const size_t array_bytes = len - off;
    if (array_bytes % OIGTL_QTDATA_ELEMENT_SIZE != 0)
        return OIGTL_ERR_MALFORMED;
    *out_count = array_bytes / OIGTL_QTDATA_ELEMENT_SIZE;
    return OIGTL_OK;
}

int oigtl_qtdata_get(const uint8_t *buf, size_t len,
                             size_t idx,
                             oigtl_qtdata_element_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;
    size_t count;
    int rc = oigtl_qtdata_count(buf, len, &count);
    if (rc != OIGTL_OK) return rc;
    if (idx >= count) return OIGTL_ERR_INVALID_ARG;
    size_t head_off = 0;
    const size_t elem_off =
        head_off + idx * OIGTL_QTDATA_ELEMENT_SIZE;
    return oigtl_qtdata_element_unpack(
        buf + elem_off, len - elem_off, out);
}

int oigtl_qtdata_pack(const oigtl_qtdata_t *msg,
                              const oigtl_qtdata_element_t *elems,
                              size_t count,
                              uint8_t *buf, size_t cap) {
    if (buf == NULL) return OIGTL_ERR_INVALID_ARG;
    if (count > 0 && elems == NULL) return OIGTL_ERR_INVALID_ARG;
    (void)msg;
    size_t off = 0;
    const size_t need = off
        + count * OIGTL_QTDATA_ELEMENT_SIZE;
    if (cap < need) return OIGTL_ERR_SHORT_BUFFER;
    for (size_t i = 0; i < count; ++i) {
        int rc2 = oigtl_qtdata_element_pack(
            &elems[i], buf + off, cap - off);
        if (rc2 < 0) return rc2;
        off += (size_t)rc2;
    }
    return (int)off;
}
