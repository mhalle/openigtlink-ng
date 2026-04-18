/* ASCII / null-padded string helpers used by message pack/unpack.
 *
 * IGTL wire strings of declared length N are null-padded: the actual
 * string occupies the leading K <= N bytes, then zero bytes fill to
 * N. Validation rejects any non-null byte after the first null
 * (matching core-cpp / core-py / core-ts semantics).
 */
#ifndef OIGTL_C_ASCII_H
#define OIGTL_C_ASCII_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compute the payload length of a null-padded fixed-size string.
 *
 * `src` points at a `width`-byte field on the wire. Returns the
 * index of the first null byte (strlen-equivalent, bounded by
 * `width`), or a negative OIGTL_ERR_MALFORMED if any non-null
 * byte appears after the first null — which indicates either
 * corrupted framing or an incorrectly-padded sender.
 *
 * Returns a non-negative `size_t`-compatible length on success
 * (0 <= result <= width), or a negative error code.
 */
int oigtl_null_padded_length(const uint8_t *src, size_t width);

/* Copy a view-string into a caller-supplied buffer, null-terminated.
 *
 * Typical use inside a user's application after `..._unpack()` has
 * populated a `const char *`-style view into the wire buffer:
 *
 *     char owned[64];
 *     oigtl_copy_string(msg.status_message,
 *                       msg.status_message_len,
 *                       owned, sizeof owned);
 *
 * Returns bytes written (not including the terminating NUL),
 * or OIGTL_ERR_SHORT_BUFFER if `dst_cap <= src_len`.
 */
int oigtl_copy_string(const char *src, size_t src_len,
                      char *dst, size_t dst_cap);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_ASCII_H */
