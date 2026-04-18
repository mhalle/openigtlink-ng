/* libFuzzer entry point for SENSOR unpack + round-trip.
 *
 * SENSOR exercises sibling-count variable primitive arrays:
 * the `larray` field (uint8, max 255) tells unpack how many
 * float64s follow. Round-trip: pack(unpack(data)) must equal
 * the first N bytes of data where N = 10 + larray*8.
 *
 * Buffer cap of 10 + 255*8 = 2050 matches the spec maximum;
 * oversized fuzzer inputs are trimmed out via the size check.
 */

#include <stdint.h>
#include <string.h>

#include "oigtl_c/messages/sensor.h"

#define FUZZ_SENSOR_CAP 2050

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > FUZZ_SENSOR_CAP) return 0;

    oigtl_sensor_t a;
    if (oigtl_sensor_unpack(data, size, &a) != OIGTL_OK) return 0;

    uint8_t buf[FUZZ_SENSOR_CAP];
    int n = oigtl_sensor_pack(&a, buf, sizeof buf);
    if (n < 0) __builtin_trap();
    if ((size_t)n > size || memcmp(buf, data, (size_t)n) != 0) __builtin_trap();
    return 0;
}
