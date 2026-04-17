# C++ wire codec codegen — plan

**Status: complete.** All six phases landed. Kept in the tree as
historical record of the implementation's phase structure; see
[`README.md`](README.md) for the current state.

Implementation commits:
- `17887c1` — this plan document
- `84a7adb` — Phases 1–6 (runtime + 84 generated messages + oracle
  + dispatch + CI) + perf work (slice-by-8 CRC) + multi-region
  fixtures + cross-language oracle parity
- `9378c96` — core-py / typed Python codec (symmetric to core-cpp)

Acceptance criterion from below was met: every supported upstream
fixture round-trips byte-for-byte, and the cross-language oracle
parity test confirms C++ and Python reference codec agree on all
23 of 24 fixtures (the 24th is `rtpwrapper.h`, an RTP-wrapper
variant rather than a regular IGTL message).

---

## Goal

Generate a C++17 wire codec (pack / unpack / dispatch) from the 84
schemas under `spec/schemas/`. Acceptance criterion: the C++ codec
round-trips every one of the 24 upstream test fixtures byte-for-byte,
matching the Python oracle's output exactly.

Transport layer is out of scope for this plan — this is codec only.

## Architecture

```
spec/schemas/*.json   (source of truth — unchanged)
        │
        ▼
corpus-tools/src/oigtl_corpus_tools/codegen/
  ├── cpp_types.py      schema type → C++17 type mapping
  ├── cpp_emit.py       Jinja2 template renderer
  └── templates/
      ├── message.hpp.jinja
      ├── message.cpp.jinja
      └── dispatch.cpp.jinja
        │
        ▼  (invoked via `oigtl-corpus codegen cpp --out core-cpp/include/oigtl/messages/`)
        │
core-cpp/
  ├── include/oigtl/
  │   ├── runtime/        hand-written, ~500 LoC
  │   │   ├── crc64.hpp
  │   │   ├── header.hpp
  │   │   ├── byte_order.hpp
  │   │   ├── error.hpp
  │   │   ├── extended_header.hpp
  │   │   ├── metadata.hpp
  │   │   └── dispatch.hpp
  │   └── messages/       GENERATED, one file per type_id
  │       ├── transform.hpp
  │       ├── image.hpp
  │       └── ...         (84 headers)
  ├── src/
  │   ├── runtime/        hand-written .cpp counterparts
  │   └── messages/       GENERATED .cpp
  ├── tests/
  │   ├── upstream_fixtures_test.cpp   round-trip all 24 upstream fixtures
  │   ├── oracle_parity_test.cpp       byte-exact match against Python oracle
  │   └── unit/           per-field-type tests
  └── CMakeLists.txt
```

## Type mapping (schema → C++17)

| Schema | C++17 |
|---|---|
| `uint8` | `std::uint8_t` |
| `uint16` | `std::uint16_t` |
| `uint32` | `std::uint32_t` |
| `uint64` | `std::uint64_t` |
| `int8`..`int64` | `std::int8_t`..`std::int64_t` |
| `float32` | `float` |
| `float64` | `double` |
| `fixed_string[N]` | `std::array<char, N>` (null-padded) |
| `fixed_bytes[N]` | `std::array<std::uint8_t, N>` |
| `length_prefixed_string` | `std::string` |
| `trailing_string` | `std::string` |
| `array<T>` count fixed | `std::array<T, N>` |
| `array<T>` sibling count | `std::vector<T>` |
| `array<T>` count_from=remaining | `std::vector<T>` |
| `array<uint8>` any var count | `std::vector<std::uint8_t>` |
| `struct` element | nested `struct Element` in the message class |

## Key design decisions

### Decided

- **C++17 public API.** No `std::span`, no `std::expected`, no concepts
  in public headers. Internal `.cpp` may use C++20 guarded by feature
  macros (matches README policy).

- **Byte-buffer first, not struct casting.** The generated class holds
  logical values in host order. Pack/unpack does explicit
  big-endian conversion via hand-written `byte_order.hpp`. No
  `#pragma pack(1)`. No type-punning. This avoids the class of bugs
  U-7 / U-8 caught in the upstream (alignment-dependent reads,
  scalar_type switches that silently truncate).

- **No external dependencies in the codec.** The runtime (crc64,
  header, byte_order, error, dispatch) uses stdlib only. ASIO / OpenSSL
  come in at the transport layer (later project).

- **Throw on error in `unpack()`.** Simple API, matches STL and Qt
  ecosystems. Also emit `try_unpack()` returning
  `std::variant<Message, Error>` for callers in trust-sensitive paths.
  Generated code produces both.

- **Dispatch via `std::unordered_map<std::string_view, UnpackFn>`.**
  Populated at static-init by the generated `dispatch_register()`.
  Amortized O(1). Extensibility: an app can register additional
  `type_id`s at runtime.

- **Codegen via Jinja2 in corpus-tools.** Keeps Python as the
  authoritative code-producing language. Generated files carry a
  "GENERATED from `spec/schemas/<name>.json` — do not edit" banner.

- **CRC-64 table ported verbatim from `igtl_util.c`.** 256 `constexpr`
  entries, same algorithm, same ECMA-182 polynomial. Our Python codec
  already verified bit-exact against upstream.

### Open

- **Optional header fast-path.** For the hot cases (TRANSFORM, a fixed
  48-byte body, 12 `getFloat32` calls), should we emit an unrolled
  pack/unpack or a loop? Loop is more generated-code-hygienic; unroll
  may be a perf win. Defer — benchmark first.

- **Metadata decode depth.** Option A: expose `std::vector<MetadataEntry>`
  (same as our Python oracle). Option B: expose `std::map<std::string,
  MetadataValue>` (convenient but loses duplicate-key ordering).
  Recommend A (match Python, spec-faithful).

- **ImageMessage pixel ownership.** Copy on unpack (safe, matches
  spec) or expose a `std::span`/view into the original buffer
  (zero-copy, bigger perf win but lifetime pitfalls). Recommend
  copy-on-unpack initially with an opt-in `unpack_view()` escape
  hatch later.

- **Header+metadata handling boundary.** Does the per-message class
  know about extended header and metadata, or does a wrapper type
  (`Envelope` / `WireMessage`) sit between? Current Python design:
  wrapper (`verify_wire_bytes` in oracle.py). Recommend mirror.

## Validation strategy

The strongest test: parity with the Python oracle.

```
tests/oracle_parity_test.cpp:
  for each fixture F in Testing/igtlutil/*.h:
    cpp_result   = Cpp::Oracle::verify_wire_bytes(F.bytes)
    python_result = subprocess("oigtl-corpus verify F")
    ASSERT_EQ(cpp_result.ok, python_result.ok)
    ASSERT_EQ(cpp_result.type_id, python_result.type_id)
    ASSERT_EQ(cpp_result.round_trip_body, F.original_body)
```

By construction, if this passes, the C++ codec is as correct as the
Python oracle — which we already proved round-trips all 24 fixtures.
Any C++ bug manifests here.

Additional test layers:

1. **Per-field-type unit tests** — one test per schema field-kind
   (primitive, fixed_string, length_prefixed, trailing_string,
   array-fixed, array-sibling, array-remaining, struct-element).
   Uses a tiny hand-crafted schema rather than a real one.

2. **Malformed input tests** — the C++ unpackers should reject the
   same malformed-input cases U-1..U-10 flagged in v3.md. Fixed fuzz
   corpus of ~50 malformed messages derived from the audit findings.

3. **Cross-language round-trip** — Python packs, C++ unpacks, Python
   re-packs, verify byte equality. And vice-versa. Detects subtle
   encoding disagreements.

## Phased work

Each phase is self-contained and merges independently.

### Phase 1 — Hand-write runtime + one message (1 day)

- `runtime/crc64.hpp/.cpp` — 256-entry table, one CRC function.
  Copy table from `codec/crc64.py`.
- `runtime/byte_order.hpp` — `read_be_u16/32/64`, `write_be_*`,
  `read_be_f32/64`. All inline.
- `runtime/header.hpp/.cpp` — 58-byte Header struct + pack/unpack +
  CRC verify. Copy layout from `codec/header.py`.
- `runtime/error.hpp` — typed exception hierarchy:
  `oigtl::error::ProtocolError`, `CrcMismatchError`,
  `UnknownMessageTypeError`, `MalformedMessageError`.
- `messages/transform.hpp/.cpp` — **hand-written** TRANSFORM class
  as the reference shape for codegen output.
- `tests/upstream_fixtures_test.cpp` — verify TRANSFORM fixture
  round-trips. Wire in CMake + a tiny test runner (no gtest dep
  initially; a hand-rolled `TEST()` macro is fine for this phase).

**Exit criterion:** hand-written TRANSFORM round-trips the upstream
`igtl_test_data_transform.h` fixture byte-for-byte.

### Phase 2 — Codegen driver + templates; regenerate TRANSFORM (1 day)

- `corpus-tools/src/oigtl_corpus_tools/codegen/cpp_types.py` — the
  type mapping table above, as a function.
- `corpus-tools/src/oigtl_corpus_tools/codegen/cpp_emit.py` — Jinja2
  renderer. Inputs: a loaded `MessageSchema` (Pydantic). Outputs:
  `.hpp` and `.cpp` strings.
- `templates/message.hpp.jinja`, `templates/message.cpp.jinja`.
- `commands/codegen.py` — CLI subcommand:
  `oigtl-corpus codegen cpp --out <dir>` regenerates all messages.
- Regenerate TRANSFORM and diff against Phase 1 hand-written.

**Exit criterion:** generated TRANSFORM passes the same fixture test.
Hand-written TRANSFORM deleted; generator is now authoritative.

### Phase 3 — Extend templates for variable-length fields (1-2 days)

Handle in order:

- `fixed_string` → STATUS's `error_name`, SENSOR's `unit`, TDATA's
  `name`, ... (widely used, simplest extension)
- `trailing_string` → STATUS's `status_message`
- `length_prefixed_string` → STRING's `value`
- `fixed_bytes` → (rare; used by CAPABILITY I think — verify)
- Arrays with fixed count (primitive element) → TRANSFORM's matrix
  (already in Phase 2), POSITION's position[3]
- Arrays with sibling-field count (primitive) → SENSOR's
  `data[larray]`, COMMAND's `command[length]`
- Arrays with `count_from=remaining` (primitive) → IMAGE's pixels,
  NDARRAY's data, COLORT's table

**Exit criterion:** STATUS, STRING, SENSOR, IMAGE, NDARRAY, COLORT,
COMMAND, POSITION all round-trip their upstream fixtures.

### Phase 4 — Struct elements (1 day)

- Emit nested `struct Element {}` for struct element types.
- Arrays of struct elements → `std::vector<Element>`, with each
  element unpacked via a generated `unpack_element(bytes, offset)`
  helper.
- Recursion: struct elements with nested arrays.

Covers: POINT, TRAJECTORY, TDATA, QTDATA, IMGMETA, LBMETA, VIDEOMETA,
CAPABILITY (fixed_string element), POLYDATA (struct points +
attribute_headers), BIND (header_entries struct).

**Exit criterion:** all 20 data messages round-trip their fixtures.

### Phase 5 — Extended header + metadata framing (1 day)

- `runtime/extended_header.hpp/.cpp` — parse/emit v3 extended header
  (12+ bytes, `ext_header_size`, `metadata_header_size`,
  `metadata_size`, `message_id`).
- `runtime/metadata.hpp/.cpp` — parse/emit metadata index + body.
  Exposed as `std::vector<MetadataEntry>` to mirror Python.
- Wrap everything in an `oracle::verify_wire_bytes(bytes)` function
  analogous to the Python oracle. Handles the full pipeline:
  header → CRC → framing split → dispatch → unpack → repack →
  byte-compare.

**Exit criterion:** Format2 fixtures (transformFormat2, tdataFormat2,
etc.) and videometa all round-trip.

### Phase 6 — Dispatch + build system + CI (1 day)

- `runtime/dispatch.hpp/.cpp` — registry mapping `string_view` →
  `UnpackFn`. Generator produces a
  `oigtl::messages::register_all(Registry&)` function that populates
  all 84 entries.
- Toplevel CMakeLists with clear target layout:
  - `oigtl_runtime` — the hand-written runtime, static library
  - `oigtl_messages` — the generated messages, static library
  - `oigtl` — the umbrella interface target
  - `oigtl_tests` — the test binary
- GitHub Actions workflow: build with GCC 11 + Clang 15 + MSVC
  latest, run tests, upload coverage.
- `oracle_parity_test.cpp` driver that shells out to the Python
  oracle and cross-checks every fixture.

**Exit criterion:** `cmake --build && ctest` passes on 3 platforms.
Oracle parity passes for all 24 fixtures.

## Estimates

Total: **6–7 focused days.** Each phase is a clean merge candidate.
No phase depends on transport work (that's a separate project
starting from Phase 6 completion).

## Resuming after compaction

Key facts the next session will want:

1. All 84 schemas under `spec/schemas/` are authoritative. Validated
   by Pydantic models under
   `corpus-tools/src/oigtl_corpus_tools/schema/`.
2. The Python oracle (`codec/oracle.py`) is the parity target —
   round-trip verified against 24 upstream fixtures.
3. CRC-64 table to port: in `codec/crc64.py`, taken verbatim from
   `igtl_util.c` at pinned SHA `94244fe`.
4. Upstream wire type_ids are in
   `reference-libs/openigtlink-upstream/Source/igtlMessageFactory.cxx`
   (authoritative for which type_ids the upstream actually emits).
5. Phase 1 starts with `core-cpp/include/oigtl/runtime/crc64.hpp`.
6. Upstream test fixtures extracted by
   `codec/test_vectors.py` — the C++ tests should consume the same
   byte arrays (trivial to emit to a header from the extractor).

Total added C++ lines projected: ~500 hand-written + ~10000 generated
(for 84 message types × ~120 lines average).
