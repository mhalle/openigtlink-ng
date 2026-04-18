/* libFuzzer entry point for POINT struct-array unpack + round-trip.
 *
 * Exercises the array-of-struct code path — count / get /
 * element_unpack / element_pack. Adversarial input is fed into
 * `oigtl_point_count`; any accepted count then walks through
 * `_get` for each element and round-trips that element through
 * `element_pack` / `element_unpack`, asserting bit-equality.
 *
 * Buffer capped at 8 KB (enough for ~60 POINT elements) to keep
 * libFuzzer exploration focused. Larger inputs are dropped.
 */

#include <stdint.h>
#include <string.h>

#include "oigtl_c/messages/point.h"

#define FUZZ_POINT_CAP 8192

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > FUZZ_POINT_CAP) return 0;

    size_t count = 0;
    if (oigtl_point_count(data, size, &count) != OIGTL_OK) return 0;

    /* Walk every element. Element round-trip: get → pack →
     * element_unpack → expect bit-equality on the materialized
     * struct. */
    for (size_t i = 0; i < count; ++i) {
        oigtl_point_element_t a;
        if (oigtl_point_get(data, size, i, &a) != OIGTL_OK) __builtin_trap();

        uint8_t ebuf[OIGTL_POINT_ELEMENT_SIZE];
        int n = oigtl_point_element_pack(&a, ebuf, sizeof ebuf);
        if (n != (int)OIGTL_POINT_ELEMENT_SIZE) __builtin_trap();

        oigtl_point_element_t b;
        if (oigtl_point_element_unpack(ebuf, sizeof ebuf, &b) != OIGTL_OK)
            __builtin_trap();

        /* memcmp of the whole struct is safe: no view pointers,
         * all fixed-width fields. The null-terminator logic in
         * the fixed_string members also guarantees the trailing
         * bytes beyond the strlen are NUL, which matches on both
         * sides. */
        if (memcmp(&a, &b, sizeof a) != 0) __builtin_trap();
    }
    return 0;
}
