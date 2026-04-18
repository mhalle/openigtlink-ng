/* libFuzzer entry point for POINT struct-array unpack + round-trip.
 *
 * Exercises the array-of-struct code path — count / get /
 * element_unpack / element_pack. Adversarial input is fed into
 * `oigtl_point_count`; any accepted count then walks through
 * `_get` for each element. Elements that parse successfully get
 * round-tripped through `element_pack` / `element_unpack`,
 * asserting bit-equality.
 *
 * Important: a successful `_count` only proves the body size is
 * a multiple of the element size. Per-element validation (e.g.
 * "group_name is a well-formed null-padded string") happens
 * inside `_get`, so a `_get` returning OIGTL_ERR_MALFORMED for
 * a bogus element is legitimate — NOT a finding. Only failures
 * on a successfully-unpacked element indicate a codec bug.
 *
 * Buffer capped at 8 KB (~60 POINT elements) to keep fuzzer
 * exploration focused. Larger inputs are dropped.
 */

#include <stdint.h>
#include <string.h>

#include "oigtl_c/messages/point.h"

#define FUZZ_POINT_CAP 8192

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > FUZZ_POINT_CAP) return 0;

    size_t count = 0;
    if (oigtl_point_count(data, size, &count) != OIGTL_OK) return 0;

    for (size_t i = 0; i < count; ++i) {
        oigtl_point_element_t a;
        /* Malformed per-element contents (e.g. junk after the
         * null-padded terminator in a fixed_string) cause _get
         * to return an error. That's the codec working as
         * designed; move on rather than trapping. */
        if (oigtl_point_get(data, size, i, &a) != OIGTL_OK) continue;

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
