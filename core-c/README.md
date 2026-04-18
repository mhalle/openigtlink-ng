# oigtl_c (core-c)

Minimal C codec for the OpenIGTLink protocol — intended for
embedded consumers (MCUs, IoT devices, resource-constrained hosts)
that can't afford the 20–50 KB linker footprint of the C++ or
Python ports but still need wire-compatible IGTL communication.

## Scope — round 1

### What ships

- **Hand-written runtime** (`src/crc64.c`, `src/ascii.c`,
  `src/header.c`, `include/oigtl_c/*.h`) — shared pieces every
  message needs. Single-table CRC-64 (2 KiB ROM, vs the 16 KiB
  slice-by-8 table in core-cpp) for embedded-friendly footprint.
- **58-byte header pack / unpack**, `oigtl_header_t` struct,
  CRC-verify helper.
- **Byte-order helpers** for all 10 primitive widths (u8, i8,
  u16, i16, u32, i32, u64, i64, f32, f64) as `static inline`
  functions — callers pay for only the widths they use,
  `--gc-sections` discards the rest.
- **Generated per-message C** (one `.c`/`.h` pair per supported
  message type) — in flight; the codegen target lands in a
  follow-up commit.

### What doesn't

- **No transport.** The codec reads / writes bytes; sockets are
  the user's problem. A device streaming TRANSFORM at 60 Hz
  does roughly this:
  ```c
  oigtl_transform_t tx = { .matrix = {...} };
  uint8_t body[OIGTL_TRANSFORM_BODY_SIZE];
  oigtl_transform_pack(&tx, body, sizeof body);
  uint8_t hdr[OIGTL_HEADER_SIZE];
  oigtl_header_pack(hdr, sizeof hdr, 2, "TRANSFORM", "Tool",
                    igtl_now(), body, sizeof body);
  send_all(sock, hdr, sizeof hdr);
  send_all(sock, body, sizeof body);
  ```
- **No heap allocation anywhere.** All buffers are caller-owned.
  Variable-length fields (trailing strings, variable primitive
  arrays) are delivered via views into the input buffer.
- **No v2 metadata**, no struct-element arrays (POINT, TDATA,
  POLYDATA etc.). Deferred to later rounds.
- **No `find_package(oigtl_c)` yet**, no packaging story. Users
  copy the .c/.h files into their project or add this dir as
  a CMake subdirectory.

## Allocation model

Everything is caller-owned, buffers passed by pointer + capacity.
Variable-length fields follow the **view pattern** — after
`*_unpack()` returns, `const char *` / `const uint8_t *` fields
in the message struct point into the input buffer. Use
`oigtl_copy_string` (string) or `oigtl_copy_f32_be` / siblings
(primitive arrays) to copy into caller-supplied storage before
the wire buffer's lifetime ends.

See the design discussion in `core-cpp/WINDOWS_PLAN.md`-adjacent
work and the repo-level architecture docs for the full rationale.

## Build + test

```bash
cmake -S core-c -B core-c/build -G 'Unix Makefiles'
cmake --build core-c/build
ctest --test-dir core-c/build --output-on-failure
```

## Wire parity

Cross-port parity against the Python / C++ / TS codecs is enforced
in CI: every message type that the C codegen emits must pack the
same canonical inputs into byte-identical output. If parity breaks,
the oracle fuzzer reports a disagreement and the build fails.

The CRC-64 table is identical to the one in
`corpus-tools/.../codec/crc64.py`, which is itself ported from
upstream `igtl_util.c`. Don't change any of these without running
the oracle parity suite.
