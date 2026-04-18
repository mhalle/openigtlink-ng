/* libFuzzer entry point for STATUS unpack + round-trip.
 *
 * STATUS exercises primitives, a 20-byte null-padded fixed
 * string, and a null-terminated trailing string. Round-trip
 * invariant: pack(unpack(data)) == data (first N bytes).
 *
 * Scratch buffer sized to comfortably hold the 30-byte fixed
 * portion plus a moderate trailing string. libFuzzer inputs
 * larger than this are legal — unpack will accept them and
 * pack will produce a matching-length output; the bounded
 * buffer here only limits what we're willing to round-trip.
 */

#include <stdint.h>
#include <string.h>

#include "oigtl_c/messages/status.h"

#define FUZZ_STATUS_CAP 4096

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > FUZZ_STATUS_CAP) return 0;

    oigtl_status_t a;
    if (oigtl_status_unpack(data, size, &a) != OIGTL_OK) return 0;

    uint8_t buf[FUZZ_STATUS_CAP];
    int n = oigtl_status_pack(&a, buf, sizeof buf);
    if (n < 0) __builtin_trap();
    if ((size_t)n > size || memcmp(buf, data, (size_t)n) != 0) __builtin_trap();
    return 0;
}
