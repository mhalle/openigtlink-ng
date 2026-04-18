/* libFuzzer entry point for TRANSFORM unpack + round-trip.
 *
 * Protocol:
 *   1. Hand fuzzer bytes to oigtl_transform_unpack. If it fails,
 *      the input wasn't a valid TRANSFORM body — no finding.
 *   2. Pack the accepted struct back to a fresh buffer. Pack MUST
 *      succeed when unpack succeeded (both agree on field shapes);
 *      any failure is a generator bug.
 *   3. The re-packed bytes must equal the first N bytes of the
 *      fuzzer input, where N is the packed size. Fixed-body
 *      messages (TRANSFORM = 48) may accept larger inputs with
 *      trailing data ignored; the comparison covers exactly N.
 *
 * Any divergence, short buffer, or sanitizer trap is a finding.
 */

#include <stdint.h>
#include <string.h>

#include "oigtl_c/messages/transform.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    oigtl_transform_t a;
    if (oigtl_transform_unpack(data, size, &a) != OIGTL_OK) return 0;

    uint8_t buf[OIGTL_TRANSFORM_BODY_SIZE];
    int n = oigtl_transform_pack(&a, buf, sizeof buf);
    if (n < 0) __builtin_trap();
    if ((size_t)n > size || memcmp(buf, data, (size_t)n) != 0) __builtin_trap();
    return 0;
}
