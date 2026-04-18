/* GENERATED from spec/schemas/lbmeta.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 */

#include "oigtl_c/messages/lbmeta.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/errors.h"

/* ------------------------------------------------------------------ */
/* Element pack/unpack                                                  */
/* ------------------------------------------------------------------ */

int oigtl_lbmeta_element_pack(
    const oigtl_lbmeta_element_t *elem,
    uint8_t *buf, size_t cap) {
    if (elem == NULL || buf == NULL) return OIGTL_ERR_INVALID_ARG;
    if (cap < OIGTL_LBMETA_ELEMENT_SIZE)
        return OIGTL_ERR_SHORT_BUFFER;

    size_t eoff = 0;
    {
        size_t n = strlen(elem->name);
        if (n > 64) return OIGTL_ERR_FIELD_RANGE;
        memcpy(buf + eoff, elem->name, n);
        if (n < 64) memset(buf + eoff + n, 0, 64 - n);
        eoff += 64;
    }
    {
        size_t n = strlen(elem->device_name);
        if (n > 20) return OIGTL_ERR_FIELD_RANGE;
        memcpy(buf + eoff, elem->device_name, n);
        if (n < 20) memset(buf + eoff + n, 0, 20 - n);
        eoff += 20;
    }
    oigtl_write_be_u8(buf + eoff, elem->label);
    eoff += 1;
    oigtl_write_be_u8(buf + eoff, elem->reserved);
    eoff += 1;
    for (size_t i = 0; i < 4; ++i) {
        oigtl_write_be_u8(buf + eoff + i * 1, elem->rgba[i]);
    }
    eoff += 4;
    for (size_t i = 0; i < 3; ++i) {
        oigtl_write_be_u16(buf + eoff + i * 2, elem->size[i]);
    }
    eoff += 6;
    {
        size_t n = strlen(elem->owner);
        if (n > 20) return OIGTL_ERR_FIELD_RANGE;
        memcpy(buf + eoff, elem->owner, n);
        if (n < 20) memset(buf + eoff + n, 0, 20 - n);
        eoff += 20;
    }
    (void)eoff;
    return (int)OIGTL_LBMETA_ELEMENT_SIZE;
}

int oigtl_lbmeta_element_unpack(
    const uint8_t *buf, size_t len,
    oigtl_lbmeta_element_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;
    if (len < OIGTL_LBMETA_ELEMENT_SIZE)
        return OIGTL_ERR_SHORT_BUFFER;

    size_t eoff = 0;
    {
        int n = oigtl_null_padded_length(buf + eoff, 64);
        if (n < 0) return n;
        memcpy(out->name, buf + eoff, (size_t)n);
        out->name[n] = '\0';
        eoff += 64;
    }
    {
        int n = oigtl_null_padded_length(buf + eoff, 20);
        if (n < 0) return n;
        memcpy(out->device_name, buf + eoff, (size_t)n);
        out->device_name[n] = '\0';
        eoff += 20;
    }
    out->label = oigtl_read_be_u8(buf + eoff);
    eoff += 1;
    out->reserved = oigtl_read_be_u8(buf + eoff);
    eoff += 1;
    for (size_t i = 0; i < 4; ++i) {
        out->rgba[i] = oigtl_read_be_u8(buf + eoff + i * 1);
    }
    eoff += 4;
    for (size_t i = 0; i < 3; ++i) {
        out->size[i] = oigtl_read_be_u16(buf + eoff + i * 2);
    }
    eoff += 6;
    {
        int n = oigtl_null_padded_length(buf + eoff, 20);
        if (n < 0) return n;
        memcpy(out->owner, buf + eoff, (size_t)n);
        out->owner[n] = '\0';
        eoff += 20;
    }
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
#define _oigtl_lbmeta_HEAD_SIZE 0

int oigtl_lbmeta_count(const uint8_t *buf, size_t len,
                               size_t *out_count) {
    if (buf == NULL || out_count == NULL) return OIGTL_ERR_INVALID_ARG;
    size_t off = 0;
    (void)buf;
    if (off > len) return OIGTL_ERR_SHORT_BUFFER;
    const size_t array_bytes = len - off;
    if (array_bytes % OIGTL_LBMETA_ELEMENT_SIZE != 0)
        return OIGTL_ERR_MALFORMED;
    *out_count = array_bytes / OIGTL_LBMETA_ELEMENT_SIZE;
    return OIGTL_OK;
}

int oigtl_lbmeta_get(const uint8_t *buf, size_t len,
                             size_t idx,
                             oigtl_lbmeta_element_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;
    size_t count;
    int rc = oigtl_lbmeta_count(buf, len, &count);
    if (rc != OIGTL_OK) return rc;
    if (idx >= count) return OIGTL_ERR_INVALID_ARG;
    size_t head_off = 0;
    const size_t elem_off =
        head_off + idx * OIGTL_LBMETA_ELEMENT_SIZE;
    return oigtl_lbmeta_element_unpack(
        buf + elem_off, len - elem_off, out);
}

int oigtl_lbmeta_pack(const oigtl_lbmeta_t *msg,
                              const oigtl_lbmeta_element_t *elems,
                              size_t count,
                              uint8_t *buf, size_t cap) {
    if (buf == NULL) return OIGTL_ERR_INVALID_ARG;
    if (count > 0 && elems == NULL) return OIGTL_ERR_INVALID_ARG;
    (void)msg;
    size_t off = 0;
    const size_t need = off
        + count * OIGTL_LBMETA_ELEMENT_SIZE;
    if (cap < need) return OIGTL_ERR_SHORT_BUFFER;
    for (size_t i = 0; i < count; ++i) {
        int rc2 = oigtl_lbmeta_element_pack(
            &elems[i], buf + off, cap - off);
        if (rc2 < 0) return rc2;
        off += (size_t)rc2;
    }
    return (int)off;
}
