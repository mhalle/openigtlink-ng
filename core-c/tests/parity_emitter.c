/* Cross-port parity emitter.
 *
 * Writes the canonical wire bytes for a named test case to stdout.
 * The parity test (ctest wrapper) invokes both this emitter and a
 * Python emitter that produces the same cases via core-py, then
 * compares the outputs byte-for-byte via `cmake -E compare_files`.
 *
 * Supported cases:
 *   transform       — TRANSFORM body, rising-index float matrix
 *   status          — STATUS body with fixed-string + trailing
 *                     string
 *   position_full   — POSITION body, 28-byte full form
 *   position_only   — POSITION body, 12-byte position-only form
 *   sensor          — SENSOR body with 3 float64 readings
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oigtl_c/byte_order.h"
#include "oigtl_c/messages/position.h"
#include "oigtl_c/messages/sensor.h"
#include "oigtl_c/messages/status.h"
#include "oigtl_c/messages/string.h"
#include "oigtl_c/messages/transform.h"

/* Write `len` bytes from `buf` to stdout (binary mode). Returns 0
 * on success, 1 on error. Called once per case emission. */
static int write_stdout(const uint8_t *buf, size_t len) {
    const size_t n = fwrite(buf, 1, len, stdout);
    return n == len ? 0 : 1;
}

static int emit_transform(void) {
    oigtl_transform_t msg;
    for (int i = 0; i < 12; ++i) msg.matrix[i] = (float)(i + 1) * 0.5f;

    uint8_t buf[OIGTL_TRANSFORM_BODY_SIZE];
    int n = oigtl_transform_pack(&msg, buf, sizeof buf);
    if (n < 0) return 2;
    return write_stdout(buf, (size_t)n);
}

static int emit_status(void) {
    oigtl_status_t msg;
    msg.code = 7;
    msg.subcode = (int64_t)-42;
    /* memcpy + NUL instead of strcpy (MSVC C4996 under /W4 /WX). */
    static const char err_name[] = "HW_FAULT";
    memcpy(msg.error_name, err_name, sizeof err_name);
    const char *m = "coolant pressure low";
    msg.status_message = m;
    msg.status_message_len = strlen(m);

    uint8_t buf[128];
    int n = oigtl_status_pack(&msg, buf, sizeof buf);
    if (n < 0) return 2;
    return write_stdout(buf, (size_t)n);
}

static int emit_position_full(void) {
    oigtl_position_t msg;
    msg.position[0] = 1.0f;
    msg.position[1] = 2.0f;
    msg.position[2] = 3.0f;

    /* Build 4 big-endian float32 quaternion bytes in-place. */
    uint8_t q[16];
    const float qv[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    for (int i = 0; i < 4; ++i) oigtl_write_be_f32(q + i * 4, qv[i]);
    msg.quaternion = q;
    msg.quaternion_bytes = sizeof q;

    uint8_t buf[64];
    int n = oigtl_position_pack(&msg, buf, sizeof buf);
    if (n < 0) return 2;
    return write_stdout(buf, (size_t)n);
}

static int emit_position_only(void) {
    oigtl_position_t msg;
    msg.position[0] = 4.0f;
    msg.position[1] = 5.0f;
    msg.position[2] = 6.0f;
    msg.quaternion = NULL;
    msg.quaternion_bytes = 0;

    uint8_t buf[32];
    int n = oigtl_position_pack(&msg, buf, sizeof buf);
    if (n < 0) return 2;
    return write_stdout(buf, (size_t)n);
}

static int emit_string(void) {
    /* encoding=3 (US-ASCII) + uint16 len prefix (11) + "hello world" */
    oigtl_string_t msg;
    msg.encoding = 3;
    const char *text = "hello world";
    msg.value = text;
    msg.value_len = strlen(text);

    uint8_t buf[128];
    int n = oigtl_string_pack(&msg, buf, sizeof buf);
    if (n < 0) return 2;
    return write_stdout(buf, (size_t)n);
}

static int emit_sensor(void) {
    oigtl_sensor_t msg;
    msg.larray = 3;
    msg.status = 0;
    msg.unit = UINT64_C(0x0102030405060708);

    const double values[3] = {1.5, -2.25, 0.125};
    uint8_t data[24];
    for (int i = 0; i < 3; ++i) {
        oigtl_write_be_f64(data + i * 8, values[i]);
    }
    msg.data = data;
    msg.data_bytes = sizeof data;

    uint8_t buf[64];
    int n = oigtl_sensor_pack(&msg, buf, sizeof buf);
    if (n < 0) return 2;
    return write_stdout(buf, (size_t)n);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr,
                "usage: %s <case>\n"
                "cases: transform, status, position_full,"
                " position_only, sensor\n",
                argv[0]);
        return 2;
    }
    const char *c = argv[1];
    if      (strcmp(c, "transform")      == 0) return emit_transform();
    else if (strcmp(c, "status")         == 0) return emit_status();
    else if (strcmp(c, "position_full")  == 0) return emit_position_full();
    else if (strcmp(c, "position_only")  == 0) return emit_position_only();
    else if (strcmp(c, "sensor")         == 0) return emit_sensor();
    else if (strcmp(c, "string")         == 0) return emit_string();
    fprintf(stderr, "unknown case: %s\n", c);
    return 2;
}
