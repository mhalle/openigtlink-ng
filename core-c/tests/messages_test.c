/* Round-trip tests across representative generated message shapes.
 *
 * Covers: fixed-count float array (TRANSFORM), primitives + fixed
 * string + null-terminated trailing string (STATUS), fixed-array
 * with body_size_set validation (POSITION), and sibling-count
 * view-based float array (SENSOR).
 *
 * Each subtest: build a message in-memory, pack, verify the body
 * size, unpack into a fresh struct, assert field equality.
 * Parity with Python / C++ / TS is covered separately in
 * parity_emitter.c — this file only proves the C codec is
 * internally consistent.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "oigtl_c/messages/position.h"
#include "oigtl_c/messages/sensor.h"
#include "oigtl_c/messages/status.h"
#include "oigtl_c/messages/transform.h"

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
static void test_transform_roundtrip(void) {
    fprintf(stderr, "test_transform_roundtrip\n");

    oigtl_transform_t src;
    for (int i = 0; i < 12; ++i) src.matrix[i] = (float)(i + 1) * 0.5f;

    uint8_t buf[OIGTL_TRANSFORM_BODY_SIZE];
    int n = oigtl_transform_pack(&src, buf, sizeof buf);
    REQUIRE(n == (int)OIGTL_TRANSFORM_BODY_SIZE);

    oigtl_transform_t dst;
    int rc = oigtl_transform_unpack(buf, (size_t)n, &dst);
    REQUIRE(rc == OIGTL_OK);
    for (int i = 0; i < 12; ++i) REQUIRE(src.matrix[i] == dst.matrix[i]);

    /* Pack into too-small buffer must fail. */
    uint8_t small[10];
    REQUIRE(oigtl_transform_pack(&src, small, sizeof small)
            == OIGTL_ERR_SHORT_BUFFER);

    /* Unpack from too-short buffer must fail. */
    REQUIRE(oigtl_transform_unpack(buf, 10, &dst)
            == OIGTL_ERR_SHORT_BUFFER);
}

/* ------------------------------------------------------------------ */
static void test_status_roundtrip(void) {
    fprintf(stderr, "test_status_roundtrip\n");

    oigtl_status_t src;
    src.code = 7;
    src.subcode = (int64_t)-42;
    /* memcpy + explicit NUL instead of strcpy — MSVC's C4996
     * treats strcpy as "unsafe" under /W4 /WX. */
    static const char err_name[] = "HW_FAULT";
    memcpy(src.error_name, err_name, sizeof err_name);
    const char *msg = "coolant pressure low";
    src.status_message = msg;
    src.status_message_len = strlen(msg);

    /* Packed size = 2 + 8 + 20 + len(msg) + 1 (null terminator) */
    const int expected_size = 2 + 8 + 20 + (int)strlen(msg) + 1;
    REQUIRE(oigtl_status_packed_size(&src) == expected_size);

    uint8_t buf[128];
    int n = oigtl_status_pack(&src, buf, sizeof buf);
    REQUIRE(n == expected_size);

    oigtl_status_t dst;
    int rc = oigtl_status_unpack(buf, (size_t)n, &dst);
    REQUIRE(rc == OIGTL_OK);
    REQUIRE(dst.code == src.code);
    REQUIRE(dst.subcode == src.subcode);
    REQUIRE(strcmp(dst.error_name, "HW_FAULT") == 0);
    REQUIRE(dst.status_message_len == strlen(msg));
    REQUIRE(memcmp(dst.status_message, msg, strlen(msg)) == 0);

    /* Verify the view points into the wire buffer. */
    REQUIRE(dst.status_message >= (const char *)buf);
    REQUIRE(dst.status_message < (const char *)(buf + sizeof buf));

    /* Missing trailing NUL must be rejected. */
    uint8_t tampered[128];
    memcpy(tampered, buf, (size_t)n);
    tampered[n - 1] = 'X';
    REQUIRE(oigtl_status_unpack(tampered, (size_t)n, &dst)
            == OIGTL_ERR_MALFORMED);
}

/* ------------------------------------------------------------------ */
static void test_status_error_name_too_long(void) {
    fprintf(stderr, "test_status_error_name_too_long\n");

    oigtl_status_t src;
    src.code = 1;
    src.subcode = 0;
    /* Overflow the wire-width cap (20 bytes) while staying within
     * the struct's [21]-byte buffer: write 21 non-NUL bytes by hand
     * so clang's fortify-source doesn't catch the intentional-too-
     * long strcpy at compile time. */
    memset(src.error_name, 'A', sizeof src.error_name);
    src.error_name[sizeof src.error_name - 1] = '\0';
    /* `error_name` now holds 20 'A' bytes + NUL — right at the cap,
     * which should PASS. Bump to 21 to force OIGTL_ERR_FIELD_RANGE. */
    src.status_message = "";
    src.status_message_len = 0;
    uint8_t tight_buf[64];
    int ok_n = oigtl_status_pack(&src, tight_buf, sizeof tight_buf);
    REQUIRE(ok_n > 0);

    /* Now break it: 21 non-NUL chars, no terminator in sight. */
    char huge[32];
    memset(huge, 'B', sizeof huge - 1);
    huge[sizeof huge - 1] = '\0';
    /* Copy 21 bytes by memcpy (no NUL in the error_name buffer). */
    memcpy(src.error_name, huge, 21);    /* overflows wire cap 20 */

    uint8_t buf[64];
    REQUIRE(oigtl_status_pack(&src, buf, sizeof buf)
            == OIGTL_ERR_FIELD_RANGE);
}

/* ------------------------------------------------------------------ */
static void test_position_roundtrip_full(void) {
    fprintf(stderr, "test_position_roundtrip_full\n");

    oigtl_position_t src;
    src.position[0] = 1.0f; src.position[1] = 2.0f; src.position[2] = 3.0f;
    /* The quaternion is a variable-count view. Build the wire bytes
     * by packing a full 28-byte body from a 4-element source array.
     * The view-pack path wants bytes-in-wire-order, so we pack to a
     * scratch buffer via a throwaway POSITION with a 3-byte
     * position_only body? No — test full path: the struct's
     * `quaternion` view needs big-endian bytes, but we're going to
     * unpack back. Simplest: round-trip through pack. Skip the
     * direct view construction and pack a 28-byte body by assigning
     * the quaternion pointer to 16 big-endian bytes prepared with
     * write_be_f32. */

    uint8_t q_bytes[16];
    for (int i = 0; i < 4; ++i) {
        const float v = (float)(i + 1) * 0.25f;
        /* Inline the same big-endian write the codegen uses. */
        uint32_t bits;
        memcpy(&bits, &v, sizeof bits);
        q_bytes[i * 4 + 0] = (uint8_t)(bits >> 24);
        q_bytes[i * 4 + 1] = (uint8_t)(bits >> 16);
        q_bytes[i * 4 + 2] = (uint8_t)(bits >>  8);
        q_bytes[i * 4 + 3] = (uint8_t)(bits      );
    }
    src.quaternion = q_bytes;
    src.quaternion_bytes = 16;

    REQUIRE(oigtl_position_packed_size(&src) == 28);

    uint8_t buf[64];
    int n = oigtl_position_pack(&src, buf, sizeof buf);
    REQUIRE(n == 28);

    oigtl_position_t dst;
    int rc = oigtl_position_unpack(buf, (size_t)n, &dst);
    REQUIRE(rc == OIGTL_OK);
    REQUIRE(dst.position[0] == 1.0f);
    REQUIRE(dst.position[1] == 2.0f);
    REQUIRE(dst.position[2] == 3.0f);
    REQUIRE(dst.quaternion_bytes == 16);
    REQUIRE(memcmp(dst.quaternion, q_bytes, 16) == 0);

    /* Malformed body size → refused. */
    REQUIRE(oigtl_position_unpack(buf, 20, &dst) == OIGTL_ERR_MALFORMED);
}

static void test_position_roundtrip_position_only(void) {
    fprintf(stderr, "test_position_roundtrip_position_only\n");

    oigtl_position_t src;
    src.position[0] = 4.0f; src.position[1] = 5.0f; src.position[2] = 6.0f;
    src.quaternion = NULL;
    src.quaternion_bytes = 0;

    REQUIRE(oigtl_position_packed_size(&src) == 12);

    uint8_t buf[32];
    int n = oigtl_position_pack(&src, buf, sizeof buf);
    REQUIRE(n == 12);

    oigtl_position_t dst;
    int rc = oigtl_position_unpack(buf, (size_t)n, &dst);
    REQUIRE(rc == OIGTL_OK);
    REQUIRE(dst.position[0] == 4.0f);
    REQUIRE(dst.quaternion_bytes == 0);
}

/* ------------------------------------------------------------------ */
static void test_sensor_roundtrip(void) {
    fprintf(stderr, "test_sensor_roundtrip\n");

    /* 3 float64 readings encoded as big-endian bytes. */
    const double values[3] = {1.5, -2.25, 0.125};
    uint8_t data_bytes[24];
    for (int i = 0; i < 3; ++i) {
        uint64_t bits;
        memcpy(&bits, &values[i], sizeof bits);
        for (int b = 0; b < 8; ++b) {
            data_bytes[i * 8 + b] = (uint8_t)(bits >> (56 - b * 8));
        }
    }

    oigtl_sensor_t src;
    src.larray = 3;
    src.status = 0;
    src.unit = UINT64_C(0x0102030405060708);
    src.data = data_bytes;
    src.data_bytes = 24;

    /* 10 header bytes + 24 data = 34. */
    REQUIRE(oigtl_sensor_packed_size(&src) == 34);

    uint8_t buf[64];
    int n = oigtl_sensor_pack(&src, buf, sizeof buf);
    REQUIRE(n == 34);

    oigtl_sensor_t dst;
    int rc = oigtl_sensor_unpack(buf, (size_t)n, &dst);
    REQUIRE(rc == OIGTL_OK);
    REQUIRE(dst.larray == 3);
    REQUIRE(dst.status == 0);
    REQUIRE(dst.unit == UINT64_C(0x0102030405060708));
    REQUIRE(dst.data_bytes == 24);
    REQUIRE(memcmp(dst.data, data_bytes, 24) == 0);

    /* Too-short body (declared larray=3 but only 2 float64s in body). */
    uint8_t truncated[18];
    memcpy(truncated, buf, sizeof truncated);
    REQUIRE(oigtl_sensor_unpack(truncated, sizeof truncated, &dst)
            == OIGTL_ERR_SHORT_BUFFER);
}

int main(void) {
    test_transform_roundtrip();
    test_status_roundtrip();
    test_status_error_name_too_long();
    test_position_roundtrip_full();
    test_position_roundtrip_position_only();
    test_sensor_roundtrip();

    if (g_fail == 0) {
        fprintf(stderr, "oigtl_c_messages: all passed\n");
        return 0;
    }
    fprintf(stderr, "oigtl_c_messages: %d failure(s)\n", g_fail);
    return 1;
}
