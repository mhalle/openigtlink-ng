/* 58-byte header pack/unpack. See header.h for layout. */

#include "oigtl_c/header.h"

#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/crc64.h"
#include "oigtl_c/errors.h"

int oigtl_header_unpack(const uint8_t *buf, size_t len,
                        oigtl_header_t *out) {
    if (buf == NULL || out == NULL) return OIGTL_ERR_INVALID_ARG;
    if (len < OIGTL_HEADER_SIZE)    return OIGTL_ERR_SHORT_BUFFER;

    out->version   = oigtl_read_be_u16(buf + 0);

    /* type_id: 12-byte null-padded field. */
    {
        const int n = oigtl_null_padded_length(buf + 2, OIGTL_TYPE_ID_WIDTH);
        if (n < 0) return n;
        memcpy(out->type_id, buf + 2, (size_t)n);
        out->type_id[n] = '\0';
        /* Zero the rest defensively so struct comparisons behave. */
        for (size_t i = (size_t)n + 1; i <= OIGTL_TYPE_ID_WIDTH; ++i) {
            out->type_id[i] = '\0';
        }
    }

    /* device_name: 20-byte null-padded field. */
    {
        const int n = oigtl_null_padded_length(buf + 14,
                                               OIGTL_DEVICE_NAME_WIDTH);
        if (n < 0) return n;
        memcpy(out->device_name, buf + 14, (size_t)n);
        out->device_name[n] = '\0';
        for (size_t i = (size_t)n + 1;
             i <= OIGTL_DEVICE_NAME_WIDTH; ++i) {
            out->device_name[i] = '\0';
        }
    }

    out->timestamp = oigtl_read_be_u64(buf + 34);
    out->body_size = oigtl_read_be_u64(buf + 42);
    out->crc       = oigtl_read_be_u64(buf + 50);
    return OIGTL_OK;
}

/* Helper: write an ASCII C-string into a null-padded field of width
 * `width`. Rejects strings longer than `width`. */
static int write_null_padded(uint8_t *dst, size_t width,
                             const char *s) {
    if (s == NULL) return OIGTL_ERR_INVALID_ARG;
    const size_t n = strlen(s);
    if (n > width) return OIGTL_ERR_FIELD_RANGE;
    if (n > 0) memcpy(dst, s, n);
    /* Pad with zeros. */
    for (size_t i = n; i < width; ++i) dst[i] = 0;
    return OIGTL_OK;
}

int oigtl_header_pack(uint8_t *dst, size_t dst_cap,
                      uint16_t version,
                      const char *type_id,
                      const char *device_name,
                      uint64_t timestamp,
                      const uint8_t *body, size_t body_len) {
    if (dst == NULL || type_id == NULL || device_name == NULL) {
        return OIGTL_ERR_INVALID_ARG;
    }
    if (body_len > 0 && body == NULL) return OIGTL_ERR_INVALID_ARG;
    if (dst_cap < OIGTL_HEADER_SIZE)  return OIGTL_ERR_SHORT_BUFFER;

    oigtl_write_be_u16(dst + 0, version);

    int rc = write_null_padded(dst + 2,  OIGTL_TYPE_ID_WIDTH,     type_id);
    if (rc != OIGTL_OK) return rc;
    rc = write_null_padded(dst + 14, OIGTL_DEVICE_NAME_WIDTH, device_name);
    if (rc != OIGTL_OK) return rc;

    oigtl_write_be_u64(dst + 34, timestamp);
    oigtl_write_be_u64(dst + 42, (uint64_t)body_len);
    oigtl_write_be_u64(dst + 50, oigtl_crc64(body, body_len));
    return OIGTL_OK;
}

int oigtl_header_verify_crc(const oigtl_header_t *hdr,
                            const uint8_t *body, size_t body_len) {
    if (hdr == NULL) return OIGTL_ERR_INVALID_ARG;
    const uint64_t computed = oigtl_crc64(body, body_len);
    return (computed == hdr->crc) ? OIGTL_OK : OIGTL_ERR_CRC_MISMATCH;
}
