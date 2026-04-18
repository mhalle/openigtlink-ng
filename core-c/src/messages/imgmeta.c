/* GENERATED from spec/schemas/imgmeta.json — do not edit.
 *
 * Regenerate with: uv run oigtl-corpus codegen c
 */

#include "oigtl_c/messages/imgmeta.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/errors.h"

/* ------------------------------------------------------------------ */
/* Element pack/unpack                                                  */
/* ------------------------------------------------------------------ */

int oigtl_imgmeta_element_pack(
    const oigtl_imgmeta_element_t *elem,
    uint8_t *buf, size_t cap) {
    if (elem == NULL || buf == NULL) return OIGTL_ERR_INVALID_ARG;
    if (cap < OIGTL_IMGMETA_ELEMENT_SIZE)
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
    {
        size_t n = strlen(elem->modality);
        if (n > 32) return OIGTL_ERR_FIELD_RANGE;
        memcpy(buf + eoff, elem->modality, n);
        if (n < 32) memset(buf + eoff + n, 0, 32 - n);
        eoff += 32;
    }
    {
        size_t n = strlen(elem->patient_name);
        if (n > 64) return OIGTL_ERR_FIELD_RANGE;
        memcpy(buf + eoff, elem->patient_name, n);
        if (n < 64) memset(buf + eoff + n, 0, 64 - n);
        eoff += 64;
    }
    {
        size_t n = strlen(elem->patient_id);
        if (n > 64) return OIGTL_ERR_FIELD_RANGE;
        memcpy(buf + eoff, elem->patient_id, n);
        if (n < 64) memset(buf + eoff + n, 0, 64 - n);
        eoff += 64;
    }
    oigtl_write_be_u64(buf + eoff, elem->timestamp);
    eoff += 8;
    for (size_t i = 0; i < 3; ++i) {
        oigtl_write_be_u16(buf + eoff + i * 2, elem->size[i]);
    }
    eoff += 6;
    oigtl_write_be_u8(buf + eoff, elem->scalar_type);
    eoff += 1;
    oigtl_write_be_u8(buf + eoff, elem->reserved);
    eoff += 1;
    (void)eoff;
    return (int)OIGTL_IMGMETA_ELEMENT_SIZE;
}

int oigtl_imgmeta_element_unpack(
    const uint8_t *buf, size_t len,
    oigtl_imgmeta_element_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;
    if (len < OIGTL_IMGMETA_ELEMENT_SIZE)
        return OIGTL_ERR_SHORT_BUFFER;

    /* Zero the whole out struct so trailing bytes after a short
     * null-padded string are deterministic rather than whatever
     * the caller's stack held. Otherwise two unpacks of the same
     * bytes into different stack frames produce structs that are
     * logically equal but differ under memcmp. */
    memset(out, 0, sizeof *out);
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
    {
        int n = oigtl_null_padded_length(buf + eoff, 32);
        if (n < 0) return n;
        memcpy(out->modality, buf + eoff, (size_t)n);
        out->modality[n] = '\0';
        eoff += 32;
    }
    {
        int n = oigtl_null_padded_length(buf + eoff, 64);
        if (n < 0) return n;
        memcpy(out->patient_name, buf + eoff, (size_t)n);
        out->patient_name[n] = '\0';
        eoff += 64;
    }
    {
        int n = oigtl_null_padded_length(buf + eoff, 64);
        if (n < 0) return n;
        memcpy(out->patient_id, buf + eoff, (size_t)n);
        out->patient_id[n] = '\0';
        eoff += 64;
    }
    out->timestamp = oigtl_read_be_u64(buf + eoff);
    eoff += 8;
    for (size_t i = 0; i < 3; ++i) {
        out->size[i] = oigtl_read_be_u16(buf + eoff + i * 2);
    }
    eoff += 6;
    out->scalar_type = oigtl_read_be_u8(buf + eoff);
    eoff += 1;
    out->reserved = oigtl_read_be_u8(buf + eoff);
    eoff += 1;
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
#define _oigtl_imgmeta_HEAD_SIZE 0

int oigtl_imgmeta_count(const uint8_t *buf, size_t len,
                               size_t *out_count) {
    if (buf == NULL || out_count == NULL) return OIGTL_ERR_INVALID_ARG;
    size_t off = 0;
    (void)buf;
    if (off > len) return OIGTL_ERR_SHORT_BUFFER;
    const size_t array_bytes = len - off;
    if (array_bytes % OIGTL_IMGMETA_ELEMENT_SIZE != 0)
        return OIGTL_ERR_MALFORMED;
    *out_count = array_bytes / OIGTL_IMGMETA_ELEMENT_SIZE;
    return OIGTL_OK;
}

int oigtl_imgmeta_get(const uint8_t *buf, size_t len,
                             size_t idx,
                             oigtl_imgmeta_element_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;
    size_t count;
    int rc = oigtl_imgmeta_count(buf, len, &count);
    if (rc != OIGTL_OK) return rc;
    if (idx >= count) return OIGTL_ERR_INVALID_ARG;
    size_t head_off = 0;
    const size_t elem_off =
        head_off + idx * OIGTL_IMGMETA_ELEMENT_SIZE;
    return oigtl_imgmeta_element_unpack(
        buf + elem_off, len - elem_off, out);
}

int oigtl_imgmeta_pack(const oigtl_imgmeta_t *msg,
                              const oigtl_imgmeta_element_t *elems,
                              size_t count,
                              uint8_t *buf, size_t cap) {
    if (buf == NULL) return OIGTL_ERR_INVALID_ARG;
    if (count > 0 && elems == NULL) return OIGTL_ERR_INVALID_ARG;
    (void)msg;
    size_t off = 0;
    const size_t need = off
        + count * OIGTL_IMGMETA_ELEMENT_SIZE;
    if (cap < need) return OIGTL_ERR_SHORT_BUFFER;
    for (size_t i = 0; i < count; ++i) {
        int rc2 = oigtl_imgmeta_element_pack(
            &elems[i], buf + off, cap - off);
        if (rc2 < 0) return rc2;
        off += (size_t)rc2;
    }
    return (int)off;
}
