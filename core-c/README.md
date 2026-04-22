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

See [`ARCHITECTURE.md`](../ARCHITECTURE.md) for the repo-level
rationale tying the embedded C codec to the other language cores.

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

## Safety contract

Split between what the codec guarantees and what the caller owns.
The split matters — C's safety surface extends past the library.

**Codec guarantees** — fuzz-verified under libFuzzer + ASan +
UBSan in CI, one target per field-shape category covering
TRANSFORM / STATUS / POSITION / SENSOR, ~120 million iterations
per run:

- Any `(buf, len)` input to `*_unpack()` returns without OOB
  read, OOB write, or undefined behavior.
- Malformed wire bytes produce a negative `OIGTL_ERR_*` code;
  never UB, never a silent garbage struct.
- `pack(unpack(x))` is byte-identical to the first N bytes of
  the original input, where N is the pack size. No silent data
  corruption under round-trip.
- A successful `*_pack()` writes exactly `packed_size()` bytes
  into the destination and never past `cap`.

**Caller's responsibility** — documented here, not enforced by
the codec:

- **Lifetime.** Variable-length fields in the unpacked struct
  (`const char *` trailing strings, `const uint8_t *` view
  arrays) point into the source wire buffer. Copy via
  `oigtl_copy_string()` / a manual `memcpy` before the wire
  buffer's lifetime ends if you need to persist them.
- **Aliasing.** Don't pass overlapping pointers for `buf` and
  `msg->*` fields. The codec doesn't detect aliasing; UB if
  you break it.
- **Return codes.** Check the return of `*_unpack()` before
  reading struct fields. Reading after a failure is undefined.
- **Thread safety.** The codec itself is reentrant (no globals,
  no allocation). Whether your caller code is safe to share
  structs across threads is your problem.

The current targets (`fuzz_point`, `fuzz_position`,
`fuzz_sensor`, `fuzz_status`, `fuzz_transform`) live under
[`fuzz/`](fuzz/). Planned future coverage (struct-element arrays,
metadata) expands the same pattern — see
[`../security/README.md`](../security/README.md) for how the
fuzzer fits into the broader security harness.
