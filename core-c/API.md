# `oigtl_c` C API tour

This is a guided tour of the `oigtl_c` C codec — what's in it,
how the pieces relate, when to use which. It sits between the
quick examples in [`README.md`](README.md) and per-symbol
documentation in the headers themselves.

For message-level questions ("what fields does TRANSFORM have?"),
see [`../spec/MESSAGES.md`](../spec/MESSAGES.md).

## What this is, and what it isn't

`oigtl_c` is the embedded codec. It packs and unpacks bytes; it
**does not** do I/O. There's no client, no server, no socket
abstraction — sockets are your code. The library is for devices
where the C++/Python ports' linker footprint or runtime
dependencies are too much (microcontrollers, IoT bridges,
ultrasound probes, devices with kilobytes of RAM).

Compared to the other cores:

| Feature | core-cpp / core-py / core-ts | core-c |
|---|---|---|
| Wire codec | yes | yes |
| TCP / WebSocket transport | yes | **no** — your code |
| Heap allocation | yes | **no** — caller-owned buffers |
| v2/v3 metadata | yes | not yet |
| Variable-rank arrays in messages | yes | not yet |

The boundary is deliberate: core-c carries only what an embedded
target actually needs. If you can afford C++17 or Python, prefer
core-cpp / core-py.

## Layered structure

```
┌──────────────────────────────────────────────────────┐
│  oigtl_c/messages/<type>.h    ← what you'll use      │
│  Per-message structs and pack/unpack functions —     │
│  oigtl_transform_t, oigtl_transform_pack/unpack,     │
│  …per supported message type.                        │
└────────────────────┬─────────────────────────────────┘
                     │ uses
┌────────────────────┴─────────────────────────────────┐
│  oigtl_c/header.h                                    │
│  58-byte header pack/unpack, CRC verify.             │
└────────────────────┬─────────────────────────────────┘
                     │ uses
┌────────────────────┴─────────────────────────────────┐
│  oigtl_c/byte_order.h, crc64.h, ascii.h, errors.h    │
│  Static-inline helpers for the 10 primitive widths,  │
│  CRC-64, ASCII validation, error codes.              │
└──────────────────────────────────────────────────────┘
```

Each layer depends only on the ones below. A typical embedded
flow uses all three; a low-level test harness might use only the
helpers.

## Sending and receiving: the typical flow

There's no transport, so the user code holds the socket. Pattern:

```c
#include "oigtl_c/header.h"
#include "oigtl_c/messages/transform.h"

int send_pose(int sock, const float matrix[12]) {
    /* 1. Build the body. */
    oigtl_transform_t tx;
    memcpy(tx.matrix, matrix, sizeof tx.matrix);
    uint8_t body[OIGTL_TRANSFORM_BODY_SIZE];
    int n = oigtl_transform_pack(&tx, body, sizeof body);
    if (n < 0) return n;                          /* OIGTL_ERR_* */

    /* 2. Build the 58-byte header (computes CRC over body). */
    uint8_t hdr[OIGTL_HEADER_SIZE];
    int rc = oigtl_header_pack(hdr, sizeof hdr,
                               /*version=*/2,
                               "TRANSFORM", "Tool",
                               igtl_now_timestamp(),
                               body, (size_t)n);
    if (rc < 0) return rc;

    /* 3. Send the bytes. Sockets are your code. */
    if (send_all(sock, hdr,  sizeof hdr) < 0) return -1;
    if (send_all(sock, body, (size_t)n)  < 0) return -1;
    return 0;
}

int receive_pose(int sock, float matrix_out[12]) {
    /* 1. Read 58 bytes for the header. */
    uint8_t hdr[OIGTL_HEADER_SIZE];
    if (recv_all(sock, hdr, sizeof hdr) < 0) return -1;

    oigtl_header_t parsed;
    int rc = oigtl_header_unpack(hdr, sizeof hdr, &parsed);
    if (rc < 0) return rc;

    /* 2. Dispatch on type_id. */
    if (strcmp(parsed.type_id, "TRANSFORM") != 0) {
        /* Skip body or handle other type. */
        return -1;
    }

    /* 3. Read body_size bytes, verify CRC, unpack. */
    if (parsed.body_size > MY_MAX_BODY) return -1;
    uint8_t body[OIGTL_TRANSFORM_BODY_SIZE];
    if (parsed.body_size != sizeof body) return -1;
    if (recv_all(sock, body, sizeof body) < 0) return -1;
    if (oigtl_header_verify_crc(&parsed, body, sizeof body) < 0)
        return -1;

    oigtl_transform_t tx;
    rc = oigtl_transform_unpack(body, sizeof body, &tx);
    if (rc < 0) return rc;

    memcpy(matrix_out, tx.matrix, sizeof tx.matrix);
    return 0;
}
```

That's the whole pattern: pack body, pack header, write bytes;
read header, validate, read body, unpack. A device streaming
TRANSFORM at 60 Hz looks identical, just in a loop.

## Working with messages: `oigtl_c/messages/<type>.h`

Per-message header generated from the schema. One pair per
supported message type (TRANSFORM, STATUS, POSITION, SENSOR, and
the others as the codegen rolls them out — see
[`README.md`](README.md) for current coverage).

Each message header exposes:

```c
/* Compile-time body size if fixed (constant), or omitted if variable. */
#define OIGTL_<TYPE>_BODY_SIZE  ((size_t)<n>)

/* The message struct — fixed-size primitives + view pointers
 * for variable-length fields. */
typedef struct oigtl_<type> {
    /* spec-aligned fields */
} oigtl_<type>_t;

/* Compute the body size needed to pack `msg`. Constant for
 * fixed-body types; computed for variable-body types. */
int oigtl_<type>_packed_size(const oigtl_<type>_t *msg);

/* Pack `msg` into `buf` (capacity `cap`). Returns bytes written
 * on success, or a negative OIGTL_ERR_* code. */
int oigtl_<type>_pack(const oigtl_<type>_t *msg,
                       uint8_t *buf, size_t cap);

/* Unpack `len` bytes at `buf` into `out`. Returns 0 on success,
 * or a negative OIGTL_ERR_* code. Variable-length fields in *out
 * are VIEWS into *buf — see "Allocation model" below. */
int oigtl_<type>_unpack(const uint8_t *buf, size_t len,
                         oigtl_<type>_t *out);
```

For per-message field details, see
[`../spec/MESSAGES.md`](../spec/MESSAGES.md).

### Allocation model — the view pattern

`oigtl_c` does no heap allocation. Variable-length fields
(trailing strings, primitive arrays in messages that have them)
in an unpacked struct are **views** into the input buffer:
`const char *` / `const uint8_t *` pointers into `buf`. They're
valid only as long as `buf` is valid.

If you need to persist a variable-length field beyond the wire
buffer's lifetime, copy:

- For ASCII strings, use `oigtl_copy_string(view, view_len, dst, dst_cap)`.
- For primitive arrays, `memcpy` into caller-owned storage.

The codec never allocates, never copies on your behalf, and never
takes ownership of memory you pass it.

## Header and CRC: `oigtl_c/header.h`

The 58-byte OpenIGTLink header — same format every protocol
version — together with CRC-64 computation and verification.

```c
#include "oigtl_c/header.h"

#define OIGTL_HEADER_SIZE          ((size_t)58)
#define OIGTL_TYPE_ID_WIDTH        ((size_t)12)
#define OIGTL_DEVICE_NAME_WIDTH    ((size_t)20)

typedef struct oigtl_header {
    uint16_t version;                                /* 1, 2, or 3 */
    char     type_id[OIGTL_TYPE_ID_WIDTH + 1];       /* null-terminated */
    char     device_name[OIGTL_DEVICE_NAME_WIDTH + 1];
    uint64_t timestamp;
    uint64_t body_size;
    uint64_t crc;
} oigtl_header_t;

/* Pack a 58-byte header. Computes the CRC over (body, body_len). */
int oigtl_header_pack(uint8_t *dst, size_t dst_cap,
                      uint16_t version,
                      const char *type_id,
                      const char *device_name,
                      uint64_t timestamp,
                      const uint8_t *body, size_t body_len);

/* Parse a 58-byte header. Validates version (1/2/3) and ASCII
 * type_id / device_name. Does NOT verify the CRC. */
int oigtl_header_unpack(const uint8_t *buf, size_t len,
                        oigtl_header_t *out);

/* Verify the parsed header's CRC matches crc64(body, body_len). */
int oigtl_header_verify_crc(const oigtl_header_t *hdr,
                            const uint8_t *body, size_t body_len);
```

Header parse is independent of message type — typical receivers
parse the header first, dispatch on `type_id`, then read +
unpack the body.

## Reaching deeper: helpers

You usually never call these directly — the per-message and
header functions wrap them — but they're available if you need
to hand-build a frame, write a fuzz harness, or work with raw
bytes outside the codec.

| Header | What lives here |
|---|---|
| `oigtl_c/byte_order.h` | `static inline` big-endian read/write helpers for u8/i8/u16/i16/u32/i32/u64/i64/f32/f64. Pay only for what you call (`--gc-sections` discards the rest). |
| `oigtl_c/crc64.h` | `oigtl_crc64(buf, len)` — single-table CRC-64-ECMA, 2 KiB ROM. Same polynomial / initial value as upstream. |
| `oigtl_c/ascii.h` | `oigtl_null_padded_length(...)` and `oigtl_copy_string(...)` — ASCII validation + view-to-owned copy. |
| `oigtl_c/errors.h` | `OIGTL_ERR_*` negative error codes returned by every `*_pack` / `*_unpack` / helper. Listed below. |

### Error codes

Every codec function returns 0 (or a positive byte count, for
`*_pack` / `*_packed_size`) on success, or a negative error
code:

| Code | Meaning |
|---|---|
| `OIGTL_OK` | 0 — success. |
| `OIGTL_ERR_SHORT_BUFFER` | Caller's destination buffer is too small. |
| `OIGTL_ERR_MALFORMED` | Wire bytes don't conform to the spec (bad version, non-ASCII in declared-ASCII field, NUL after non-NUL in fixed-string padding, etc.). |
| `OIGTL_ERR_CRC_MISMATCH` | Header's CRC doesn't match the computed CRC over the body. |
| `OIGTL_ERR_INVALID_ARG` | NULL pointer, zero capacity, etc. — caller bug. |
| `OIGTL_ERR_FIELD_RANGE` | A field value at pack time is out of spec range (string too long for fixed-string field, etc.). |

Check the return code before reading struct fields. Reading after
a failure is undefined.

## Build + link

`core-c` is a small CMake project — see [`README.md`](README.md)
for the build commands. The library is `liboigtl_c.a`, with
public headers under `include/oigtl_c/`. No transitive
dependencies; links cleanly with newlib, picolibc, or full glibc.

Embedded callers typically compile only the .c files for the
message types they actually use, plus the runtime (`crc64.c`,
`ascii.c`, `header.c`). The CMake target `oigtl_c_messages` is
the convenience all-types static library; for tighter footprint,
list only the message sources you need.

## Where to look for what

| You want to… | Look at |
|---|---|
| Pack a TRANSFORM and ship it over your own socket | The pattern in "Sending and receiving" above |
| Find out what a TRANSFORM looks like on the wire | [`../spec/MESSAGES.md`](../spec/MESSAGES.md) |
| Verify a received frame's CRC before unpacking the body | `oigtl_header_verify_crc` in `oigtl_c/header.h` |
| Persist a variable-length view beyond the wire buffer's lifetime | "Allocation model — the view pattern" above |
| Understand what the codec promises and what's your responsibility | [`README.md`](README.md) §"Safety contract" |
| Tune the linker footprint | [`README.md`](README.md) (don't pull every message TU; the `--gc-sections` story) |
| Understand the cross-language guarantees | [`../spec/CONFORMANCE.md`](../spec/CONFORMANCE.md) |
