/* c-upstream verifier — independent low-level differential-check
 * participant using upstream OpenIGTLink's igtlutil C layer.
 *
 * Role: one more reference codec alongside py-ref (the
 * authoritative oracle) and our py / cpp / ts implementations.
 * Upstream's C layer is one step closer to the wire than its
 * C++ wrapper (the C++ layer calls into C for byte-level work),
 * which makes it a uniquely low-level verifier — it reveals
 * where upstream's C code diverges from spec.
 *
 * Terminology note: the differential runner plumbs this through
 * its OracleSubprocess machinery alongside the other
 * implementations, and the user-facing flag is `--oracle
 * c-upstream`. That's the pre-existing CLI naming. In strict
 * testing terminology this is a *verifier*, not an authoritative
 * oracle; py-ref and the upstream test fixtures are the oracle.
 *
 * Protocol — same as the other differential-check CLIs:
 *   stdin:  one hex-encoded wire-byte string per line.
 *   stdout: one compact JSON object per line, identical shape to
 *           core-cpp/src/oracle_cli.cpp:
 *           { "ok", "type_id", "device_name", "version",
 *             "body_size", "ext_header_size", "metadata_count",
 *             "round_trip_ok", "error" }.
 *   EOF:    clean exit.
 *
 * Scope (round 1):
 *   - Supports TRANSFORM and STATUS.
 *   - v1 framing only (bare body). Upstream's igtlutil C layer
 *     has no extended-header / metadata codec — that's in the
 *     C++ layer. For v2/v3-framed messages the verifier emits
 *     an ok=false report with a version-mismatch error string;
 *     the differential runner treats that as "participant out of
 *     scope" rather than a disagreement.
 *   - Unsupported type_ids emit ok=false with error="unsupported
 *     type_id"; not a disagreement, just a skip.
 *
 * This CLI links against upstream's own C sources (compiled into
 * liboigtl_c_upstream_static in core-cpp/CMakeLists.txt) plus a
 * minimal igtlConfigure.h stand-in in this directory — no
 * dependency on upstream's CMake configure step.
 */

#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "igtl_header.h"
#include "igtl_status.h"
#include "igtl_transform.h"

/* --------------------------------------------------------------
 * Bounded buffers. Every wire frame we process fits in 64 KiB
 * plus header; allocate enough for the common case and refuse
 * anything larger.
 * -------------------------------------------------------------- */

#define MAX_WIRE_BYTES  (1024 * 1024)  /* 1 MiB — ample for fuzzer */

typedef struct {
    int          ok;
    const char  *type_id;       /* null-terminated, points into header */
    char         device_name[IGTL_HEADER_NAME_SIZE + 1];
    uint16_t     version;
    uint64_t     body_size;
    int          round_trip_ok;
    char         error[128];
} report_t;

/* --------------------------------------------------------------
 * Hex → bytes. Strict parse: only [0-9a-fA-F], even length. No
 * whitespace tolerated.
 * -------------------------------------------------------------- */

static int hex_nibble(char c, int *out) {
    if (c >= '0' && c <= '9') { *out = c - '0'; return 1; }
    if (c >= 'a' && c <= 'f') { *out = c - 'a' + 10; return 1; }
    if (c >= 'A' && c <= 'F') { *out = c - 'A' + 10; return 1; }
    return 0;
}

static int hex_to_bytes(const char *hex, size_t hex_len,
                        uint8_t *out, size_t out_cap, size_t *out_len) {
    if (hex_len % 2 != 0) return 0;
    if (hex_len / 2 > out_cap) return 0;
    for (size_t i = 0; i < hex_len; i += 2) {
        int hi = 0, lo = 0;
        if (!hex_nibble(hex[i], &hi) || !hex_nibble(hex[i + 1], &lo))
            return 0;
        out[i / 2] = (uint8_t)((hi << 4) | lo);
    }
    *out_len = hex_len / 2;
    return 1;
}

/* --------------------------------------------------------------
 * Minimal JSON emission — we only need strings, ints, bool, and
 * null. Device name + error strings need escaping.
 * -------------------------------------------------------------- */

static void json_escape(FILE *f, const char *s, size_t n) {
    fputc('"', f);
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  fputs("\\\"", f); break;
            case '\\': fputs("\\\\", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            case '\b': fputs("\\b", f); break;
            case '\f': fputs("\\f", f); break;
            default:
                if (c < 0x20 || c >= 0x7f) {
                    /* Non-ASCII bytes in the fuzzer's random
                     * inputs get escaped as \u00XX. Valid JSON,
                     * round-trips byte-identically across the
                     * oracle CLIs. */
                    fprintf(f, "\\u%04x", c);
                } else {
                    fputc(c, f);
                }
        }
    }
    fputc('"', f);
}

static void json_escape_cstr(FILE *f, const char *s) {
    json_escape(f, s, strlen(s));
}

static void emit_report(FILE *f, const report_t *r) {
    fputs("{\"ok\":", f);
    fputs(r->ok ? "true" : "false", f);
    fputs(",\"type_id\":", f);
    if (r->type_id) json_escape_cstr(f, r->type_id);
    else            fputs("\"\"", f);
    fputs(",\"device_name\":", f);
    json_escape_cstr(f, r->device_name);
    fprintf(f, ",\"version\":%u", (unsigned)r->version);
    fprintf(f, ",\"body_size\":%" PRIu64, r->body_size);
    /* ext_header_size and metadata_count are always null/0 for
     * this v1-only oracle. Keep the field names in the output so
     * the differential runner's shape-check stays happy. */
    fputs(",\"ext_header_size\":null", f);
    fputs(",\"metadata_count\":0", f);
    fputs(",\"round_trip_ok\":", f);
    fputs(r->round_trip_ok ? "true" : "false", f);
    fputs(",\"error\":", f);
    json_escape_cstr(f, r->error);
    fputs("}\n", f);
}

/* --------------------------------------------------------------
 * Per-type verifiers — CRC the body against the header's
 * recorded CRC, then round-trip-check by re-encoding.
 * -------------------------------------------------------------- */

static int verify_transform(const uint8_t *body, size_t body_size,
                            uint64_t wire_crc, report_t *r) {
    if (body_size != 48) {
        snprintf(r->error, sizeof(r->error),
                 "TRANSFORM body_size=%zu, expected 48", body_size);
        return 0;
    }
    /* Work on a mutable copy — upstream's helpers take non-const
     * pointers even for the compute-only CRC case. */
    uint8_t buf[48];
    memcpy(buf, body, 48);

    /* Upstream's get_crc just CRCs 48 bytes from the pointer,
     * regardless of their endianness. We feed it the raw wire
     * bytes (BE) directly. */
    uint64_t computed = igtl_transform_get_crc((igtl_float32 *)buf);
    if (computed != wire_crc) {
        snprintf(r->error, sizeof(r->error),
                 "TRANSFORM CRC mismatch: wire=%" PRIx64
                 " computed=%" PRIx64, wire_crc, computed);
        return 0;
    }

    /* Round-trip: decode (BE → host) then re-encode (host → BE).
     * Original bytes should match after the full round-trip. */
    igtl_transform_convert_byte_order((igtl_float32 *)buf); /* → host */
    igtl_transform_convert_byte_order((igtl_float32 *)buf); /* → BE  */
    r->round_trip_ok = (memcmp(buf, body, 48) == 0);
    return 1;
}

static int verify_status(const uint8_t *body, size_t body_size,
                         uint64_t wire_crc, report_t *r) {
    if (body_size < IGTL_STATUS_HEADER_SIZE) {
        snprintf(r->error, sizeof(r->error),
                 "STATUS body_size=%zu < %d", body_size,
                 IGTL_STATUS_HEADER_SIZE);
        return 0;
    }

    /* Copy the fixed-size status header and the trailing message
     * into a working buffer so we can mutate without touching
     * the original. */
    uint8_t buf[MAX_WIRE_BYTES];
    if (body_size > sizeof(buf)) {
        snprintf(r->error, sizeof(r->error), "STATUS body too large");
        return 0;
    }
    memcpy(buf, body, body_size);

    /* CRC computed on BE status header + message-length-prefixed
     * msg bytes. Upstream's helper takes the header + (msglen,
     * msg) tuple. The message portion starts after the fixed
     * status-header bytes. */
    const igtl_uint32 msg_len =
        (igtl_uint32)(body_size - IGTL_STATUS_HEADER_SIZE);
    const char *msg = (const char *)(buf + IGTL_STATUS_HEADER_SIZE);
    uint64_t computed = igtl_status_get_crc(
        (igtl_status_header *)buf, msg_len, msg);
    if (computed != wire_crc) {
        snprintf(r->error, sizeof(r->error),
                 "STATUS CRC mismatch: wire=%" PRIx64
                 " computed=%" PRIx64, wire_crc, computed);
        return 0;
    }

    /* Round-trip: flip to host, flip back, expect byte match. */
    igtl_status_convert_byte_order((igtl_status_header *)buf);
    igtl_status_convert_byte_order((igtl_status_header *)buf);
    r->round_trip_ok = (memcmp(buf, body, body_size) == 0);
    return 1;
}

/* --------------------------------------------------------------
 * Entry: parse header, dispatch. Populates report_t.
 * -------------------------------------------------------------- */

static void process_one(const uint8_t *wire, size_t n, report_t *r) {
    memset(r, 0, sizeof(*r));
    r->type_id = NULL;
    r->device_name[0] = '\0';

    if (n < IGTL_HEADER_SIZE) {
        snprintf(r->error, sizeof(r->error),
                 "truncated: %zu < 58", n);
        return;
    }

    /* Copy header so we can byte-swap in place. */
    igtl_header hdr;
    memcpy(&hdr, wire, IGTL_HEADER_SIZE);
    igtl_header_convert_byte_order(&hdr);

    r->version = hdr.header_version;
    r->body_size = hdr.body_size;

    /* Null-terminate the type_id and device_name in place on the
     * scratch copy, then stash them in the report. Upstream's
     * header struct has fixed-size char arrays with no guaranteed
     * null terminator. */
    static char type_id_buf[IGTL_HEADER_TYPE_SIZE + 1];
    memcpy(type_id_buf, hdr.name, IGTL_HEADER_TYPE_SIZE);
    type_id_buf[IGTL_HEADER_TYPE_SIZE] = '\0';
    r->type_id = type_id_buf;

    memcpy(r->device_name, hdr.device_name, IGTL_HEADER_NAME_SIZE);
    r->device_name[IGTL_HEADER_NAME_SIZE] = '\0';

    /* v1 only in round 1. v2/v3 messages use an extended header
     * + metadata region that upstream's C layer doesn't parse. */
    if (hdr.header_version != 1) {
        snprintf(r->error, sizeof(r->error),
                 "version %u out of scope (v1 only)",
                 (unsigned)hdr.header_version);
        return;
    }

    if (n < IGTL_HEADER_SIZE + hdr.body_size) {
        snprintf(r->error, sizeof(r->error),
                 "body truncated: have %zu, need %" PRIu64,
                 n - IGTL_HEADER_SIZE, hdr.body_size);
        return;
    }

    const uint8_t *body = wire + IGTL_HEADER_SIZE;

    if (strncmp(hdr.name, "TRANSFORM", IGTL_HEADER_TYPE_SIZE) == 0) {
        if (verify_transform(body, (size_t)hdr.body_size, hdr.crc, r)) {
            r->ok = 1;
        }
    } else if (strncmp(hdr.name, "STATUS", IGTL_HEADER_TYPE_SIZE) == 0) {
        if (verify_status(body, (size_t)hdr.body_size, hdr.crc, r)) {
            r->ok = 1;
        }
    } else {
        snprintf(r->error, sizeof(r->error),
                 "unsupported type_id: %.*s",
                 (int)IGTL_HEADER_TYPE_SIZE, hdr.name);
    }
}

/* --------------------------------------------------------------
 * Main loop.
 * -------------------------------------------------------------- */

int main(void) {
    char *line = NULL;
    size_t cap = 0;
    uint8_t *wire = (uint8_t *)malloc(MAX_WIRE_BYTES);
    if (!wire) return 2;

    for (;;) {
        ssize_t len;
#if defined(_WIN32)
        /* getline is POSIX; Windows has its own; this oracle is
         * POSIX-only for now. */
        break;
#else
        len = getline(&line, &cap, stdin);
        if (len < 0) break;
#endif
        /* Strip trailing newline. */
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0) continue;

        size_t wire_len = 0;
        report_t r;
        memset(&r, 0, sizeof(r));
        if (!hex_to_bytes(line, (size_t)len, wire, MAX_WIRE_BYTES, &wire_len)) {
            r.version = 0;
            snprintf(r.error, sizeof(r.error), "bad hex input");
        } else {
            process_one(wire, wire_len, &r);
        }
        emit_report(stdout, &r);
        fflush(stdout);
    }

    free(line);
    free(wire);
    return 0;
}
