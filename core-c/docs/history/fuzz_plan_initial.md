# core-c fuzz plan

> **Status: initial targets shipped.** Fuzz harnesses for
> `fuzz_point`, `fuzz_position`, `fuzz_sensor`, `fuzz_status`,
> and `fuzz_transform` live under
> [`core-c/fuzz/`](fuzz/) with a seed corpus. Build instructions
> in [`core-c/fuzz/README.md`](fuzz/README.md). This document is
> retained as the original design record; status of additional
> targets and coverage goals tracks below.

Originally scoped as a follow-on round after the initial C
codec + tests ship and CI stabilizes.

## Purpose

Catch codec-side memory-safety bugs on adversarial wire input,
under ASan + UBSan, within the CI budget. Explicitly out of scope:
caller-misuse bugs (view lifetime, aliasing, thread safety) —
those are documented contracts, not codec behavior, and are
unreachable by any fuzz target we could write.

## What fuzzing will catch

- **OOB reads during `*_unpack()`** on malformed wire bytes —
  bogus variable-count fields, truncated bodies, claimed body_size
  that exceeds `len`, malformed trailing-null expectations, etc.
- **OOB writes during `*_pack()`** when a struct's
  caller-supplied view fields (`msg->foo_bytes`, trailing_string
  `_len`) sum to more than the destination buffer's capacity.
- **Integer overflow on size math** — `count * elem_size`
  composed expressions that wrap on a `uint64` count.
- **Underflow on subtraction** — `bytes = len - off` when
  off > len due to an earlier parsing bug.
- **Pointer UB** — null-pointer-with-zero-length memcpy,
  signed/unsigned conversion UB, strict-aliasing issues.
- **Round-trip divergence** — `pack(unpack(x))` ≠ `x` for
  any valid wire input.

## What fuzzing cannot catch (caller contract, not codec)

- Use-after-free on view pointers after the wire buffer frees.
- Aliasing / overlapping input and output buffers.
- Thread-safety issues in caller code using our structs.
- Caller ignoring our return code and reading struct fields
  after a failure.
- Caller passing NULL where we require non-NULL (we check and
  return `OIGTL_ERR_INVALID_ARG`, but UB upstream if the caller
  dereferences our error code as a success count).

These live in the README's "safety contract" section; they're
not something a fuzz target can exercise from outside.

## Target design

### Scope: one target per representative message shape

Four libFuzzer targets, one per field-shape category:

| Target | Shapes it covers |
|---|---|
| `fuzz_transform` | Fixed body, fixed-count primitive array |
| `fuzz_status`    | Primitives + fixed null-padded string + trailing null-terminated string |
| `fuzz_position`  | `body_size_set` validation, fixed array + variable primitive array (view) |
| `fuzz_sensor`    | Sibling-count variable primitive array (view) |

Rationale: these are the four shapes we already round-trip-test.
Every other generated message uses one of these shapes in some
combination, so a bug found here tends to indicate a generator
bug that affects multiple messages. Adding fifth-through-Nth
targets has diminishing returns until we add new shapes (struct
arrays, metadata, etc.), at which point the same pattern repeats.

### Target structure: unpack-then-pack-then-unpack

Each target combines two checks in one `LLVMFuzzerTestOneInput`:

```c
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* 1. Unpack adversarial bytes. If it fails, done. */
    oigtl_transform_t a;
    if (oigtl_transform_unpack(data, size, &a) != OIGTL_OK) return 0;

    /* 2. Pack the result. Should always succeed. */
    uint8_t buf[OIGTL_TRANSFORM_BODY_SIZE];
    int n = oigtl_transform_pack(&a, buf, sizeof buf);
    if (n < 0) __builtin_trap();    // pack should never fail on valid unpack

    /* 3. Round-trip: re-unpack and require equality. */
    oigtl_transform_t b;
    if (oigtl_transform_unpack(buf, (size_t)n, &b) != OIGTL_OK)
        __builtin_trap();
    if (memcmp(&a, &b, sizeof a) != 0)
        __builtin_trap();
    return 0;
}
```

`__builtin_trap()` gives libFuzzer a crashing signal so the
fuzzer treats round-trip divergence as a finding, not a silent
pass.

This structure naturally mixes "unpack robustness" and
"round-trip correctness" into a single target without doubling
the fuzz time budget.

### Why `__builtin_trap()` rather than `assert()`?

`assert()` compiles to a no-op under `-DNDEBUG` — which is what
release builds do and what matters for testing. `__builtin_trap`
is unconditional. libFuzzer catches it via signal, prints the
crashing input, and exits non-zero. Exactly what we want.

## Build system integration

New CMake option in `core-c/CMakeLists.txt`:

```cmake
option(OIGTL_C_BUILD_FUZZERS
    "Build libFuzzer fuzz targets under ASan+UBSan (requires Clang)."
    OFF)

if(OIGTL_C_BUILD_FUZZERS)
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR
            "OIGTL_C_BUILD_FUZZERS=ON requires Clang; "
            "got ${CMAKE_C_COMPILER_ID}")
    endif()
    set(_fuzz_flags "-fsanitize=fuzzer,address,undefined -g -O1")
    foreach(_tgt fuzz_transform fuzz_status fuzz_position fuzz_sensor)
        add_executable(${_tgt} fuzz/${_tgt}.c)
        target_link_libraries(${_tgt} PRIVATE oigtl_c_messages)
        target_compile_options(${_tgt} PRIVATE ${_fuzz_flags})
        target_link_options(${_tgt} PRIVATE ${_fuzz_flags})
    endforeach()
endif()
```

Default **OFF** so regular Debug/Release builds stay clean. Fuzzers
only build on Clang + explicit opt-in, matching the core-cpp
pattern.

## Seed corpus

One Python script — `core-c/fuzz/seeds/gen.py` (PEP 723 + stdlib) —
writes a handful of canonical inputs per target:

- `transform/valid_01.bin`     — the case from `parity_emitter.c`
- `transform/zeros.bin`        — 48 zero bytes
- `transform/too_short.bin`    — 12 bytes
- `status/minimal.bin`         — code=1, subcode=0, empty fields
- `status/long_message.bin`    — 200-byte status_message
- `status/no_null_term.bin`    — missing trailing NUL
- `position/12_bytes.bin`      — position-only
- `position/24_bytes.bin`      — compressed quat
- `position/28_bytes.bin`      — full quat
- `position/malformed_20.bin`  — neither 12 / 24 / 28
- `sensor/larray_0.bin`        — empty data
- `sensor/larray_255.bin`      — max data
- `sensor/larray_mismatch.bin` — claimed larray doesn't match body

Total: ~12 seeds, <1 KB total. libFuzzer mutates outward from
here.

## CI integration

Added to the existing `fuzz-smoke` job (clang-15 on ubuntu, which
already has the right toolchain and sanitizer runtimes):

```yaml
- name: Build C fuzz targets (ASan+UBSan)
  env:
    CC: clang-15
  run: |
    cmake -S core-c -B core-c/build-fuzz \
          -DOIGTL_C_BUILD_FUZZERS=ON
    cmake --build core-c/build-fuzz --parallel

- name: Run core-c fuzz targets (30 s each)
  run: |
    for tgt in fuzz_transform fuzz_status fuzz_position fuzz_sensor; do
      core-c/build-fuzz/$tgt \
          core-c/fuzz/seeds/${tgt#fuzz_} \
          -max_total_time=30 \
          -artifact_prefix=./fuzz-artifact-c-${tgt}-
    done
```

Total CI time impact: ~2 minutes added to the fuzz-smoke job.

Artifacts from any crashes are uploaded via the existing
`fuzz-artifact-*` upload step; no workflow changes beyond the
two blocks above.

## Acceptance

- All four targets build clean under Clang + ASan + UBSan.
- Each target passes a 30-second run from the seed corpus
  without hitting a sanitizer or a `__builtin_trap`.
- Seed corpus is regeneratable via `uv run core-c/fuzz/seeds/gen.py`.
- No new runtime dependencies; no new CI jobs.
- Total added code: ~250 LoC split across 4 fuzz targets + 1 seed
  generator + CMake glue.

## Out of scope (tracked for future rounds)

- **Cross-port differential fuzzing.** Adding `--oracle c` to
  `oigtl-corpus fuzz differential` would require a C oracle CLI
  that reads wire bytes from stdin, decodes via our generated
  unpack, and emits canonical JSON for byte-level comparison
  against py-ref / py / cpp / ts. Strong property, but a
  separate project (~300 LoC + CI wiring).
- **Fuzzing future shapes** — struct-element arrays (POINT,
  TDATA, POLYDATA) and metadata. Added once the codegen
  supports them.
- **Coverage-guided corpus growth.** libFuzzer's default mutator
  is good enough for 30-second runs; if we ever want to run
  longer (e.g., nightly 1-hour sweeps), we'd want to persist
  and grow the corpus across runs.
- **Sanitizers beyond ASan+UBSan.** MSan (uninitialized memory)
  needs a special instrumented libc build that adds complexity
  for marginal additional coverage of a codec that rarely reads
  uninitialized memory. TSan (thread safety) doesn't apply —
  the codec has no threading.

## Relationship to the safety contract

The contract in the README will say:

> The codec guarantees that any `(buf, len)` input to
> `*_unpack()` returns without OOB read, OOB write, or
> undefined behavior. Malformed input always produces a
> negative error code, never UB. [Fuzz-verified under
> ASan + UBSan in CI.]

The "fuzz-verified" clause becomes true only once this plan
lands. Until then, the docs should say "tested via round-trip
assertions" to avoid overclaiming.
