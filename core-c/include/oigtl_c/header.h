/* 58-byte OpenIGTLink message header — parse and emit.
 *
 * Layout (identical across protocol versions v1/v2/v3):
 *   off  size  field
 *     0     2  version (uint16, big-endian)
 *     2    12  type_id (ascii, null-padded)
 *    14    20  device_name (ascii, null-padded)
 *    34     8  timestamp (uint64, big-endian; IGTL fixed-point)
 *    42     8  body_size (uint64, big-endian)
 *    50     8  crc (uint64, big-endian, ECMA-182 over body)
 *   total: 58
 *
 * This header is wire-version-invariant. We parse it before knowing
 * which body schema applies.
 *
 * Unlike `oigtl_*_pack()` for message bodies, header pack computes
 * the CRC from a caller-supplied body buffer and stores it at the
 * right offset. The caller owns body memory; we don't copy it.
 */
#ifndef OIGTL_C_HEADER_H
#define OIGTL_C_HEADER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OIGTL_HEADER_SIZE        ((size_t)58)
#define OIGTL_TYPE_ID_WIDTH      ((size_t)12)
#define OIGTL_DEVICE_NAME_WIDTH  ((size_t)20)

/* Parsed header. Strings are null-terminated in this struct; wire
 * null-padding is consumed by the parser. */
typedef struct oigtl_header {
    uint16_t version;
    char     type_id[OIGTL_TYPE_ID_WIDTH + 1];
    char     device_name[OIGTL_DEVICE_NAME_WIDTH + 1];
    uint64_t timestamp;
    uint64_t body_size;
    uint64_t crc;
} oigtl_header_t;

/* Parse a 58-byte header. Returns OIGTL_OK (0) on success, or:
 *   OIGTL_ERR_INVALID_ARG  if `buf` is NULL or `out` is NULL.
 *   OIGTL_ERR_SHORT_BUFFER if `len` < OIGTL_HEADER_SIZE.
 *   OIGTL_ERR_MALFORMED    if type_id or device_name contains a
 *                          non-null byte after the first null.
 *
 * Does NOT verify CRC. Use oigtl_header_verify_crc() after the body
 * is available. */
int oigtl_header_unpack(const uint8_t *buf, size_t len,
                        oigtl_header_t *out);

/* Serialize a header into a 58-byte destination.
 *
 * Computes CRC-64 over (body, body_len) and stores it in
 * dst[50..58]. body_size in the emitted header is set to body_len.
 *
 * Inputs:
 *   dst, dst_cap   caller buffer; must be at least OIGTL_HEADER_SIZE.
 *   version        protocol version (1, 2, or 3).
 *   type_id        C-string, length <= OIGTL_TYPE_ID_WIDTH.
 *   device_name    C-string, length <= OIGTL_DEVICE_NAME_WIDTH.
 *   timestamp      IGTL fixed-point timestamp (see spec).
 *   body           pointer to already-packed body bytes.
 *   body_len       length of body in bytes.
 *
 * Returns OIGTL_OK (0) on success, or:
 *   OIGTL_ERR_INVALID_ARG  for NULL dst / type_id / device_name,
 *                          or NULL body when body_len > 0.
 *   OIGTL_ERR_SHORT_BUFFER if dst_cap < OIGTL_HEADER_SIZE.
 *   OIGTL_ERR_FIELD_RANGE  if type_id or device_name is too long.
 */
int oigtl_header_pack(uint8_t *dst, size_t dst_cap,
                      uint16_t version,
                      const char *type_id,
                      const char *device_name,
                      uint64_t timestamp,
                      const uint8_t *body, size_t body_len);

/* Verify the header's CRC matches crc64(body, body_len). Returns
 * OIGTL_OK or OIGTL_ERR_CRC_MISMATCH. */
int oigtl_header_verify_crc(const oigtl_header_t *hdr,
                            const uint8_t *body, size_t body_len);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_HEADER_H */
