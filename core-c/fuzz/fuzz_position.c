/* libFuzzer entry point for POSITION unpack + round-trip.
 *
 * POSITION exercises the `body_size_set` validator — only
 * bodies of exactly 12 / 24 / 28 bytes are accepted. The
 * quaternion tail is a view-based variable primitive array.
 * Round-trip: pack(unpack(data)) must equal data exactly
 * (no trailing-data case for POSITION because unpack rejects
 * anything not in the allowed size set).
 */

#include <stdint.h>
#include <string.h>

#include "oigtl_c/messages/position.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    oigtl_position_t a;
    if (oigtl_position_unpack(data, size, &a) != OIGTL_OK) return 0;

    uint8_t buf[28];    /* max body size for POSITION */
    int n = oigtl_position_pack(&a, buf, sizeof buf);
    if (n < 0) __builtin_trap();
    if ((size_t)n != size || memcmp(buf, data, (size_t)n) != 0) __builtin_trap();
    return 0;
}
