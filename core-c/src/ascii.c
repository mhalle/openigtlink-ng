/* See ascii.h for the why. Both helpers are tiny; pulling them into
 * a separate TU keeps embedded link lines honest — users who only
 * pack (and never unpack) still don't have to carry the validator.
 */

#include "oigtl_c/ascii.h"

#include <string.h>

#include "oigtl_c/errors.h"

int oigtl_null_padded_length(const uint8_t *src, size_t width) {
    if (src == NULL) {
        return OIGTL_ERR_INVALID_ARG;
    }
    /* Find the first null. */
    size_t k = 0;
    while (k < width && src[k] != 0) {
        ++k;
    }
    /* Anything after the first null must also be null, else the
     * sender has corrupted the padding. This matches the codec's
     * behavior in Python / C++ / TS. */
    for (size_t i = k; i < width; ++i) {
        if (src[i] != 0) {
            return OIGTL_ERR_MALFORMED;
        }
    }
    return (int)k;
}

int oigtl_copy_string(const char *src, size_t src_len,
                      char *dst, size_t dst_cap) {
    if (dst == NULL || dst_cap == 0) {
        return OIGTL_ERR_INVALID_ARG;
    }
    if (src_len >= dst_cap) {
        /* Need room for src + one terminating NUL. */
        if (dst_cap > 0) dst[0] = '\0';
        return OIGTL_ERR_SHORT_BUFFER;
    }
    if (src_len > 0) {
        memcpy(dst, src, src_len);
    }
    dst[src_len] = '\0';
    return (int)src_len;
}
