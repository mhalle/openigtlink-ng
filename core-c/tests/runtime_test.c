/* Smoke tests for the hand-written core-c runtime.
 *
 * Exercises: CRC-64 known vectors, header pack+unpack round-trip,
 * null-padded validator, copy-string helper. Zero dependencies
 * beyond oigtl_c_runtime and libc. Prints FAIL lines on stderr
 * and returns 1 on any failure — the ctest integration calls exit
 * code and fails loudly.
 */

#include <stdio.h>
#include <string.h>

#include "oigtl_c/ascii.h"
#include "oigtl_c/byte_order.h"
#include "oigtl_c/crc64.h"
#include "oigtl_c/errors.h"
#include "oigtl_c/header.h"

static int g_fail = 0;

#define REQUIRE(cond)                                                    \
    do {                                                                 \
        if (!(cond)) {                                                   \
            fprintf(stderr, "  FAIL %s:%d  %s\n",                        \
                    __FILE__, __LINE__, #cond);                          \
            g_fail++;                                                    \
        }                                                                \
    } while (0)

/* ------------------------------------------------------------------ */
/* CRC-64 parity vs known values                                       */
/* ------------------------------------------------------------------ */
static void test_crc64_empty(void) {
    fprintf(stderr, "test_crc64_empty\n");
    /* Matches Python / C++ / TS: zero bytes hashes to zero. */
    REQUIRE(oigtl_crc64(NULL, 0) == 0);

    const uint8_t empty = 0;
    REQUIRE(oigtl_crc64(&empty, 0) == 0);
}

static void test_crc64_abc(void) {
    fprintf(stderr, "test_crc64_abc\n");
    /* Parity vector: same input hashed by the corpus-tools Python
     * reference crc64 implementation.
     *     uv run python -c 'from oigtl_corpus_tools.codec.crc64 \
     *         import crc64; print(hex(crc64(b"abc")))'
     *     → 0x66501a349a0e0855
     * If this drifts, either the table or the algorithm is wrong. */
    const uint8_t buf[3] = {'a', 'b', 'c'};
    REQUIRE(oigtl_crc64(buf, 3) == UINT64_C(0x66501a349a0e0855));
}

/* ------------------------------------------------------------------ */
/* Byte-order round-trips — one smoke per family.                       */
/* ------------------------------------------------------------------ */
static void test_byte_order(void) {
    fprintf(stderr, "test_byte_order\n");
    uint8_t buf[8];

    oigtl_write_be_u16(buf, 0xABCD);
    REQUIRE(buf[0] == 0xAB && buf[1] == 0xCD);
    REQUIRE(oigtl_read_be_u16(buf) == 0xABCD);

    oigtl_write_be_u32(buf, 0xDEADBEEFu);
    REQUIRE(buf[0] == 0xDE && buf[3] == 0xEF);
    REQUIRE(oigtl_read_be_u32(buf) == 0xDEADBEEFu);

    oigtl_write_be_u64(buf, UINT64_C(0x0123456789ABCDEF));
    REQUIRE(buf[0] == 0x01 && buf[7] == 0xEF);
    REQUIRE(oigtl_read_be_u64(buf) == UINT64_C(0x0123456789ABCDEF));

    oigtl_write_be_f32(buf, 3.14159f);
    REQUIRE(oigtl_read_be_f32(buf) == 3.14159f);

    oigtl_write_be_f64(buf, 2.71828182845904523);
    REQUIRE(oigtl_read_be_f64(buf) == 2.71828182845904523);
}

/* ------------------------------------------------------------------ */
/* Header pack + unpack round-trip                                      */
/* ------------------------------------------------------------------ */
static void test_header_roundtrip(void) {
    fprintf(stderr, "test_header_roundtrip\n");

    const uint8_t body[4] = {1, 2, 3, 4};
    uint8_t hdr_buf[OIGTL_HEADER_SIZE];

    int rc = oigtl_header_pack(
        hdr_buf, sizeof hdr_buf,
        /*version=*/2,
        /*type_id=*/"STATUS",
        /*device_name=*/"Tracker",
        /*timestamp=*/UINT64_C(0x1234567800000000),
        body, sizeof body);
    REQUIRE(rc == OIGTL_OK);

    oigtl_header_t out;
    rc = oigtl_header_unpack(hdr_buf, sizeof hdr_buf, &out);
    REQUIRE(rc == OIGTL_OK);
    REQUIRE(out.version == 2);
    REQUIRE(strcmp(out.type_id, "STATUS") == 0);
    REQUIRE(strcmp(out.device_name, "Tracker") == 0);
    REQUIRE(out.timestamp == UINT64_C(0x1234567800000000));
    REQUIRE(out.body_size == sizeof body);

    REQUIRE(oigtl_header_verify_crc(&out, body, sizeof body) == OIGTL_OK);

    /* Tampered body → CRC mismatch. */
    const uint8_t evil[4] = {1, 2, 3, 5};
    REQUIRE(oigtl_header_verify_crc(&out, evil, sizeof evil)
            == OIGTL_ERR_CRC_MISMATCH);
}

static void test_header_short_buffer(void) {
    fprintf(stderr, "test_header_short_buffer\n");
    uint8_t small[10];
    int rc = oigtl_header_pack(small, sizeof small, 2, "X", "Y", 0, NULL, 0);
    REQUIRE(rc == OIGTL_ERR_SHORT_BUFFER);

    oigtl_header_t out;
    rc = oigtl_header_unpack(small, sizeof small, &out);
    REQUIRE(rc == OIGTL_ERR_SHORT_BUFFER);
}

static void test_header_field_range(void) {
    fprintf(stderr, "test_header_field_range\n");
    uint8_t buf[OIGTL_HEADER_SIZE];
    /* type_id width is 12; 13 chars must be rejected. */
    int rc = oigtl_header_pack(buf, sizeof buf, 2,
                               "THIRTEENCHARS", "OK", 0, NULL, 0);
    REQUIRE(rc == OIGTL_ERR_FIELD_RANGE);
}

/* ------------------------------------------------------------------ */
/* ASCII helpers                                                        */
/* ------------------------------------------------------------------ */
static void test_null_padded(void) {
    fprintf(stderr, "test_null_padded\n");
    const uint8_t ok[6] = {'A', 'B', 'C', 0, 0, 0};
    REQUIRE(oigtl_null_padded_length(ok, 6) == 3);

    const uint8_t trailing_junk[6] = {'A', 'B', 0, 'X', 0, 0};
    REQUIRE(oigtl_null_padded_length(trailing_junk, 6) == OIGTL_ERR_MALFORMED);

    const uint8_t no_null[4] = {'A', 'B', 'C', 'D'};
    REQUIRE(oigtl_null_padded_length(no_null, 4) == 4);

    /* Non-ASCII byte before the first NUL is rejected — declared-
     * ASCII string fields must stay in 0x00..0x7F. Matches
     * core-py / core-cpp / core-ts. */
    const uint8_t non_ascii[4] = {'A', 0x80, 0, 0};
    REQUIRE(oigtl_null_padded_length(non_ascii, 4) == OIGTL_ERR_MALFORMED);

    const uint8_t high_bit[4] = {0xC3, 0xA9, 0, 0};  /* UTF-8 "é" */
    REQUIRE(oigtl_null_padded_length(high_bit, 4) == OIGTL_ERR_MALFORMED);

    /* 0x7F (DEL) is the boundary — still ASCII, still accepted. */
    const uint8_t boundary[4] = {0x7F, 0, 0, 0};
    REQUIRE(oigtl_null_padded_length(boundary, 4) == 1);
}

/* Regression: protocol version must be in {1, 2, 3}. Previously the
 * parser accepted any uint16 value, diverging from core-py / -cpp /
 * -ts and from the negative-corpus contract. */
static void test_header_unsupported_version(void) {
    fprintf(stderr, "test_header_unsupported_version\n");

    /* Build a valid header, then overwrite the version field. */
    uint8_t wire[OIGTL_HEADER_SIZE];
    const int rc = oigtl_header_pack(wire, sizeof wire,
                                     /*version=*/1,
                                     "STATUS", "dev",
                                     /*timestamp=*/0,
                                     /*body=*/NULL, 0);
    REQUIRE(rc == OIGTL_OK);

    /* version=0 — never valid. */
    wire[0] = 0; wire[1] = 0;
    oigtl_header_t out;
    REQUIRE(oigtl_header_unpack(wire, sizeof wire, &out)
            == OIGTL_ERR_MALFORMED);

    /* version=4 — outside the supported set. */
    wire[0] = 0; wire[1] = 4;
    REQUIRE(oigtl_header_unpack(wire, sizeof wire, &out)
            == OIGTL_ERR_MALFORMED);

    /* version=0xFFFF — uint16 max, explicitly hostile. */
    wire[0] = 0xFF; wire[1] = 0xFF;
    REQUIRE(oigtl_header_unpack(wire, sizeof wire, &out)
            == OIGTL_ERR_MALFORMED);

    /* version=2 — spec-conformant, must still succeed. */
    wire[0] = 0; wire[1] = 2;
    REQUIRE(oigtl_header_unpack(wire, sizeof wire, &out) == OIGTL_OK);
    REQUIRE(out.version == 2);
}

static void test_copy_string(void) {
    fprintf(stderr, "test_copy_string\n");
    char dst[8];
    int rc = oigtl_copy_string("hello", 5, dst, sizeof dst);
    REQUIRE(rc == 5);
    REQUIRE(strcmp(dst, "hello") == 0);

    /* Exactly-at-capacity must fail (no room for NUL). */
    rc = oigtl_copy_string("exactly!", 8, dst, sizeof dst);
    REQUIRE(rc == OIGTL_ERR_SHORT_BUFFER);
    REQUIRE(dst[0] == '\0');
}

int main(void) {
    test_crc64_empty();
    test_crc64_abc();
    test_byte_order();
    test_header_roundtrip();
    test_header_short_buffer();
    test_header_field_range();
    test_null_padded();
    test_header_unsupported_version();
    test_copy_string();

    if (g_fail == 0) {
        fprintf(stderr, "oigtl_c_runtime: all passed\n");
        return 0;
    }
    fprintf(stderr, "oigtl_c_runtime: %d failure(s)\n", g_fail);
    return 1;
}
