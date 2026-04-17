# Security + conformance testing harness — plan

Status: **Phases 1–4 complete**; Phase 5 deferred. Durable
state for resuming work after context compaction.

**Phase 3 headline:** libFuzzer found a real integer-overflow
bug in `parse_wire` on its first 30-second run —
`kHeaderSize + body_size` wrapped `size_t` for `body_size` near
`UINT64_MAX`, letting a 58-byte malformed header drive `crc64`
past the end of the buffer. Fixed; regression pinned via
`framing_header/body_size_uint64_max.bin`.

## Resume-from-cold briefing

**You are picking up the security harness work for openigtlink-ng**
at `/Users/halazar/Dropbox/development/openigtlink-ng`. The project
has four independent wire codecs (reference Python, typed Python,
typed C++, typed TypeScript) generated from shared JSON schemas
under `spec/schemas/`. Phases 1 and 2 of a 5-phase security harness
have shipped; this file captures where things stand and what comes
next.

### Current state (fully shipped)

Recent git history (most recent first):

```
9444429 Canonical-form round-trip + residual ASCII strictness: 1M fuzz clean
c72be8f Strict ASCII in header + content string fields across all codecs
ad8683e security Phase 2: differential oracle fuzzer across five oracles
e84a64f Ship core-py numpy arrays, core-ts wire codec, and security harness Phase 1
```

### What's in place

| Component | Location | Role |
|---|---|---|
| Negative corpus (21 entries) | `spec/corpus/negative/` + `index.json` | Must-reject inputs, per-codec tests |
| Corpus generator | `corpus-tools/.../negative_corpus.py` | Deterministic builder for corpus bin files |
| `oigtl-corpus fuzz differential` | `corpus-tools/.../commands/fuzz.py` | CLI entry point |
| Fuzz generators | `corpus-tools/.../fuzz/generators.py` | random / mutate / structured |
| Fuzz runner | `corpus-tools/.../fuzz/runner.py` | Orchestrates 5 oracle subprocesses |
| C++ oracle CLI | `core-cpp/src/oracle_cli.cpp` + `oigtl_oracle_cli` binary | stdin hex → stdout JSON |
| TS oracle CLI | `core-ts/src/oracle_cli.ts` → `build-tests/src/oracle_cli.js` | same protocol, Node |
| Typed Py oracle CLI | `core-py/src/oigtl/oracle_cli.py` (run via `python -m oigtl.oracle_cli`) | same protocol, typed path |
| Shared C++ ASCII helper | `core-cpp/include/oigtl/runtime/ascii.hpp` | Used by header + generated code |
| `policy.check_body_size_set` | `corpus-tools/.../codec/policy.py` | body_size_set enforcement |
| Disagreements log | `security/disagreements/<seed>.jsonl` (gitignored) | One line per cross-oracle divergence |

### Five selectable oracles

- **py-ref** — reference dict codec, in-process (fastest: 22k it/s).
- **py** — typed Python classes, numpy path (`oigtl.messages.*`).
- **py-noarray** — typed Python, `OIGTL_NO_NUMPY=1` forces `array.array` fallback.
- **cpp** — external `oigtl_oracle_cli`.
- **ts** — external `oracle_cli.js`.

### How to run

```bash
# Build everything
cmake --build core-cpp/build --target oigtl_oracle_cli
(cd core-ts && npx tsc -p tsconfig.json --outDir build-tests)
(cd core-py && uv sync --all-extras)

# Fuzz
cd corpus-tools
uv run oigtl-corpus fuzz differential -n 100000 \
    --oracle py-ref --oracle py --oracle cpp --oracle ts \
    --progress-every 25000

# Smoke tests
(cd corpus-tools && uv run pytest -q)                   # 141 tests
(cd core-py && uv run pytest -q)                        # 170 tests
(cd core-py && OIGTL_NO_NUMPY=1 uv run pytest -q)       # 168 + 2 skip
(cd core-ts && npm test)                                # 126 tests
(cd core-cpp/build && ctest)                            # 3/3

# Drift checks (CI gates)
cd corpus-tools
uv run oigtl-corpus codegen cpp --check
uv run oigtl-corpus codegen python --check
uv run oigtl-corpus codegen ts --check
uv run oigtl-corpus fixtures export-json --check
uv run oigtl-corpus corpus generate-negative --check
uv run oigtl-corpus schema emit-meta --check
```

### Current fuzzer baseline

**0 disagreements at 1,000,000 iterations** across py-ref / py /
cpp / ts (seed 42). Same when py-noarray is added at 200k. All
four (five) codecs reject the same ~98.1% of random/mutated/structured
inputs and accept the same ~1.9%.

This means any divergence the fuzzer finds in a future run is
genuinely new — either a regression from a codec change, or a
generator class that reaches a code path the current generators
don't. That's the Phase 3 motivation.

## Phase 2 fix log (reference for future conformance debate)

Four bug classes found and fixed during Phase 2:

1. **ASCII strictness divergence in headers.** Python's
   `.decode("ascii")` rejected bytes ≥ 0x80 in type_id /
   device_name; C++ and TS accepted. Fixed by aligning all four
   codecs on strict ASCII (< 0x80). See header.cpp / header.ts.
2. **ASCII strictness in content string fields.** Same class,
   body-level. TS codegen switched from `TextDecoder("ascii")`
   (aliased to windows-1252 per the Encoding spec) to explicit
   byte-checked `_readAsciiRaw`. C++ codegen gained a shared
   `oigtl::runtime::ascii` helper used by fixed_string,
   trailing_string, and length_prefixed_string with encoding=ascii.
3. **Length-prefixed string bounds.** Reference codec's Python
   slice was lenient. Added explicit `offset + length > len(data)`
   check in `codec/fields.py`.
4. **NaN bit-pattern round-trip normalization.** Python and TS
   route float32 through FPU (quiets signaling NaNs); C++
   memcpy preserves bits. Oracles now use a canonical-form
   round-trip check: if `pack(unpack(x)) != x` but
   `pack(unpack(pack(unpack(x)))) == pack(unpack(x))` (second
   pass stable = codec reached fixed point), accept as
   canonical-form round-trip. Length mismatches stay strict.
   Implemented in ref Py / typed Py / C++ / TS oracles.

Also fixed mid-Phase-2: TS `_readAsciiNullPadded` was stripping
only trailing NULs instead of splitting at first NUL — caught
on the very first fuzzer run (29 of initial 33 disagreements).

### Design decisions worth preserving

- **Canonical-form round-trip is the contract**, not byte-exact.
  Length mismatch stays strict (extra bytes = structural bug);
  value-level normalization is tolerated when the second pass is
  stable. Schema-agnostic, robust to codec changes, correctly
  handles IEEE-754 NaN canonicalization.
- **`TextDecoder("ascii", { fatal: true })` is a trap.** Per the
  WHATWG Encoding spec, "ascii" is aliased to windows-1252 and
  never rejects bytes ≥ 0x80. Use explicit byte validation.
- **ASCII strictness is the spec-conformant choice.** No observed
  deployed device sends non-ASCII in header or content ASCII
  fields. If compatibility with a permissive sender ever becomes
  an ask, revisit per-field with a config flag.
- **`policy.check_body_size_set` is reusable** for any future
  spec-whitelist constraint beyond POSITION's {12, 24, 28}.
- **numpy / array.array paths are bit-identical** in oracle
  output — validated at 200k iterations. The `OIGTL_NO_NUMPY=1`
  env var is the testing hook.

---

## What's next: Phase 3

See the Phase 3 section below. **Scope has shifted** since the
original plan: now that the differential fuzzer is clean at 1M
iterations, the marginal value of per-codec in-process fuzzers
depends on what they'd catch that cross-language fuzzing doesn't.

**Unique value of in-process fuzzers:**

- **libFuzzer on C++** catches memory-safety issues (ASan/UBSan).
  The differential fuzzer only sees logical-level disagreements;
  an OOB read that happens to produce the same JSON output as
  another codec wouldn't be flagged. C++ is the only codec where
  memory safety is an actual concern.
- **Atheris / Python-native fuzzing** would catch unexpected
  exception types — anything uncaught that isn't a `ProtocolError`
  subclass. But this is largely already exercised by the
  differential fuzzer (the subprocess trap catches it).
- **JS fuzzing** has no real tooling; the deterministic-PRNG loop
  in the plan is a coverage hack, not real fuzzing.

**Revised recommendation:** ship libFuzzer on C++ (the real win),
defer Atheris / JS in-process fuzzers unless they surface value
during Phase 4 CI wiring. 30-minute libFuzzer smoke in CI is the
high-leverage addition.

---

## Goal

Exploit the fact that the project has **four independent codecs**
(Python reference, Python typed, C++ typed, TypeScript typed) that
must all produce identical results on valid inputs and all reject
the same set of invalid inputs. Turn this into an automated
harness that:

1. Proves each codec rejects a curated adversarial corpus.
2. Continuously fuzzes the codecs and asserts they either all
   accept a byte sequence with the same decoded semantics, or all
   reject it with a recognized error class.
3. Runs memory-safety and coverage gates on the C++ and Python
   codecs so buffer overruns, OOB reads, and uninitialized-memory
   reads fail CI instead of shipping.
4. (Stretch) Runs the same fuzzer against the pinned upstream
   reference C library to flag any divergence between our
   implementation and the deployed one.

Acceptance criterion: **`make security` runs 5-minute differential
fuzz campaigns against all four codecs and the C++ libFuzzer
targets, and the negative corpus of ≥ 30 entries is rejected by
every codec with a typed error.**

## Why now (rather than after transport)

Every codec accepts untrusted bytes from the network eventually.
Fuzzing them before they're wrapped in transport lets us fix bugs
where they live — the codec — rather than chasing them through an
async I/O layer. The differential-oracle configuration is uniquely
available to this project because of the four-codec architecture,
and loses value as soon as transport adds a second axis of
variation (timing, partial reads, reconnect).

Fuzzing transport-wrapped codecs is also strictly more expensive:
each input has to be framed, delivered, and the response observed
across a socket. In-process fuzzing runs at millions of iterations
per second against the codec alone.

## Layout

```
security/
├── PLAN.md                    (this file)
├── README.md                  (how to run fuzzers + reproduce crashes)
└── differential/              (driven by corpus-tools subcommand)
    ├── generators.py          (random, mutate-from-corpus, structured)
    └── runner.py              (feeds bytes to all 4 oracles in parallel)

spec/corpus/negative/          (curated must-reject inputs)
├── README.md                  (entry format + classification)
├── framing_header/*.bin       (58-byte header layer failures)
├── framing_ext_header/*.bin   (v2/v3 extended header failures)
├── framing_metadata/*.bin     (metadata region failures)
├── content/*.bin              (per-message-type content failures)
└── index.json                 (name → { description, expected_error_class })

core-cpp/fuzz/                 (libFuzzer entry points)
├── fuzz_header.cc
├── fuzz_oracle.cc
└── corpus/                    (seed corpus, linked from spec/corpus)

core-py/tests/
└── test_negative_corpus.py    (parametrized; every entry must reject)

core-ts/tests/
└── negative_corpus.test.ts    (same; driven by spec/corpus/negative/index.json)

corpus-tools/src/oigtl_corpus_tools/commands/
└── fuzz.py                    (new subcommand: fuzz differential / fuzz corpus)
```

## Phased work

### Phase 1 — Negative corpus curation + per-codec rejection tests (1 day)

Curate ~30 adversarial inputs covering the failure modes the
specifications + prose spec actually call out as MUST-reject, plus
the obvious trust-boundary hazards.

**Index file** `spec/corpus/negative/index.json`:

```json
{
  "format_version": 1,
  "entries": {
    "truncated_header": {
      "description": "Header with only 42 bytes, missing CRC and body_size.",
      "expected_error_class": "HeaderParseError",
      "spec_reference": "protocol/v3.md §Header",
      "path": "framing_header/truncated_42.bin"
    },
    "header_body_size_overflow": {
      "description": "Header declares body_size=2^63, no body.",
      "expected_error_class": "BodyDecodeError",
      "path": "framing_header/body_size_overflow.bin"
    },
    ...
  }
}
```

**Entry categories** (at least 4-6 per category):

- *framing_header*: short buffer, truncated CRC, body_size >
  available, version=0, version=99 (future-claimed).
- *framing_ext_header*: ext_header_size < 12, ext_header_size >
  body, metadata_size + metadata_header_size > body.
- *framing_metadata*: metadata_count implies more bytes than the
  metadata region holds, value_size points past buffer end, key
  bytes contain invalid UTF-8.
- *content/transform*: body_size != 48 (TRANSFORM is fixed-body).
- *content/position*: body_size ∉ {12, 24, 28} (spec MUST-reject).
- *content/image*: num_components×scalar_size×size[0]×size[1]×size[2]
  ≠ pixel buffer length; integer overflow on product.
- *content/ndarray*: `dim` field disagrees with `size` array length;
  `dim * size_max * scalar_size` overflows uint64.
- *content/bind*: ncmessages × entry_size > body.
- *content/sensor*: `larray × 8 ≠ data length`.
- *content/polydata*: npoints × 12 > body; strips offsets not
  monotonic.
- *content/colortable*: lut entries × scalar_size ≠ table length;
  mapping_type outside {1, 2, 3}.
- *content/string*: length_prefix > remaining; trailing null
  missing when `null_terminated=true`.

For each entry, **all four codecs** must reject with an error of
the declared class (or a subclass thereof).

**Per-codec tests:**
- `core-py/tests/test_negative_corpus.py` — parametrized pytest
  over `index.json`; calls `verify_wire_bytes` and checks the
  returned error matches the expected class.
- `core-ts/tests/negative_corpus.test.ts` — mirrors above using
  `node --test`.
- `core-cpp/tests/negative_corpus_test.cc` — CTest parametrized
  via the index.
- `corpus-tools/tests/test_negative_corpus.py` — reference codec
  path, same index.

**Exit:** all negative-corpus tests pass on all four codecs; CI
gate added.

### Phase 2 — Differential oracle driver (1 day)

New subcommand `oigtl-corpus fuzz differential`:

```bash
uv run oigtl-corpus fuzz differential \
    --iterations 100000 \
    --seed 42 \
    --generator random,mutate,structured \
    --oracles py-ref,cpp,ts
```

For each iteration, generates a candidate byte sequence, runs it
through every requested oracle, and compares the results:

- All oracles accept → semantic outputs (type_id, body_size,
  ext_header_size, metadata_count, round_trip_ok) must agree.
  Any mismatch is a bug.
- All oracles reject → error classes must agree (or at least not
  contradict — "CRC mismatch" vs "Header parse" is acceptable;
  "OK" vs "reject" is not).
- Mixed → always a bug. Logged and the input is saved as a new
  negative-corpus candidate.

**Generators:**

- *random*: uniform-random bytes, length ~ Pareto(64, α=1.5) so
  most inputs are small but occasional inputs are megabytes.
- *mutate*: take a fixture from `UPSTREAM_VECTORS`, apply one of:
  bit flip, byte insert, byte delete, chunk duplication, length
  field tamper.
- *structured*: synthesize a header with arbitrary version /
  type_id / body_size, then fuzz the body per the schema.

Oracles are invoked as subprocesses for the cross-process case
(`core-cpp/build/oigtl_oracle_binary`, `oigtl-corpus oracle
verify`, `node core-ts/build-tests/runtime/oracle_cli.js`). Each
exposes the same stable JSON report shape.

**Exit:** 1M iterations across all generators with zero
disagreements on a known-clean state of the project. Any
disagreements found before that point go into `spec/corpus/negative/`
and get fixed.

### Phase 3 — C++ libFuzzer (memory-safety gate) (0.5-1 day)

**Revised scope** (see briefing above): only the C++ side gets
in-process fuzzing. The differential runner already catches
logical-level divergences across all four codecs; what it
doesn't catch is **memory-safety bugs that produce
logically-plausible output** — an OOB read past a buffer end
that happens to return zeros still decodes as a valid message.
libFuzzer under ASan/UBSan is the only tool that catches this.

Python and TS are memory-safe by construction; their in-process
fuzzers would only catch "unexpected exception class escapes
the codec", which the subprocess-based differential runner
already flags as a protocol violation.

**core-cpp/fuzz/** with two libFuzzer entry points:

- `fuzz_header.cc`: `LLVMFuzzerTestOneInput` → `unpack_header` +
  `verify_crc`. Catches buffer overruns in the 58-byte header
  parse. Seed corpus: every 58-byte header prefix from
  `spec/corpus/negative/framing_header/` + the headers of
  `spec/corpus/upstream-fixtures.json`.
- `fuzz_oracle.cc`: `LLVMFuzzerTestOneInput` → `verify_wire_bytes`.
  Catches OOB / UAF in every per-message code path.
  Seed corpus: upstream fixtures as wire-bytes + negative corpus.

Build gate: `cmake -DBUILD_FUZZERS=ON` on clang with libFuzzer
support. Build with `-fsanitize=address,undefined,fuzzer`.

**Workflow:**

```bash
# Local development
cmake -S core-cpp -B core-cpp/build-fuzz \
      -DBUILD_FUZZERS=ON -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_COMPILER=clang++
cmake --build core-cpp/build-fuzz --target fuzz_oracle
core-cpp/build-fuzz/fuzz_oracle core-cpp/fuzz/corpus/oracle \
    -max_total_time=60

# CI smoke
# (Phase 4 wires this into .github/workflows/ci.yml)
```

**Crash handling:** any crashing input gets its hex dumped to
`security/fuzz-crashes/<sha1>.hex`, a repro test added to
`core-cpp/tests/negative_corpus_test.cpp`, and fixed.

**Exit:** Both fuzz targets run 60 seconds on the seed corpus
without an ASan / UBSan / libFuzzer report in CI.

### Phase 3b (deferred) — Python / TS in-process fuzzers

Skipped unless Phase 4 surfaces value. Reasoning:

- **Atheris on Python**: would find `struct.error` / `IndexError`
  escaping the ProtocolError hierarchy. But the differential
  runner's subprocess wrapper already catches this as an
  "uncaught exception" event (see `oracle_cli.py` exception
  wrapping). Marginal new value.
- **JS fuzzing**: no mature coverage-guided tool for pure TS.
  `jazzer.js` exists but the integration cost outweighs the
  value given JS is memory-safe.

Revisit if Phase 4 shows CI coverage gaps that only in-process
fuzzing could close.

### Phase 4 — CI wiring + coverage (0.5 days)

- New CI job `fuzz-smoke`: builds the C++ libFuzzer targets with
  ASan+UBSan, runs each for 60s with the seed corpus. Fails on
  any crash or sanitizer report.
- New CI job `coverage`: builds core-cpp with `--coverage`, runs
  the full test suite + negative corpus + differential fuzzer for
  10s, extracts gcov, enforces ≥ 90% line coverage on runtime + 85%
  on messages (generated code — lower bar tolerable).
- `pytest-cov` for core-py with the same gate.
- `c8` for core-ts.

**Exit:** The three new CI jobs are green on main.

### Phase 5 (stretch, 0.5 days) — Upstream parity fuzzer

If time permits: wire the pinned upstream reference C library as a
fifth oracle in the differential runner. Any divergence from
upstream is a wire-compat bug in our implementation, which is the
tightest possible conformance signal.

Gated on the upstream library building cleanly in CI; otherwise
local-only.

## File touch list (remaining work)

### Phase 3 (C++ libFuzzer)

**Create:**
- `core-cpp/fuzz/fuzz_header.cc`
- `core-cpp/fuzz/fuzz_oracle.cc`
- `core-cpp/fuzz/corpus/header/` — seed corpus (headers only)
- `core-cpp/fuzz/corpus/oracle/` — seed corpus (full wire messages)
- `core-cpp/fuzz/README.md` — how to build + run locally

**Modify:**
- `core-cpp/CMakeLists.txt` — add `option(BUILD_FUZZERS OFF)`, guard
  two new `add_executable` calls behind it, require Clang +
  `-fsanitize=fuzzer,address,undefined`.

### Phase 4 (CI)

**Modify:**
- `.github/workflows/ci.yml` — add a `fuzz-smoke` job (Ubuntu,
  Clang 15+, BUILD_FUZZERS=ON, runs each target for 60s against
  the seed corpus; fails on ASan/UBSan/libFuzzer report).
- Optionally a `coverage` job (gcov on C++, pytest-cov on Python,
  c8 on TS) — deferred; only worth it if the fuzz-smoke alone
  doesn't move the needle.

### Phase 5 (stretch)

**Create:**
- `corpus-tools/src/oigtl_corpus_tools/fuzz/upstream.py` — spawn
  the pinned upstream C library binary as a 6th oracle.

**Modify:**
- `.github/workflows/ci.yml` — gate upstream parity behind a
  nightly schedule (building the upstream library on every PR is
  too slow).

## Estimates (remaining)

| Phase | Scope | Duration |
|---|---|---|
| 3 — C++ libFuzzer | 2 fuzz targets + CMake gate + seed corpus | 0.5-1 day |
| 4 — CI fuzz-smoke | workflow + seed corpus curation | 0.5 day |
| 5 (stretch) — Upstream parity | subprocess glue + nightly CI | 0.5-1 day |

## Resuming after compaction

Key facts for the next session:

1. **The fuzzer baseline is clean.** `oigtl-corpus fuzz differential
   -n 1000000 --oracle py-ref --oracle py --oracle cpp --oracle ts`
   produces 0 disagreements on seed 42. Any new disagreement is
   a genuine regression — run the fuzzer before and after any
   codec change, and the `security/disagreements/<seed>.jsonl`
   log will pinpoint it.
2. **Error hierarchy** is rooted at `ProtocolError` in every codec:
   - C++: `core-cpp/include/oigtl/runtime/error.hpp`
   - Reference Py: `ValueError` (plain, for historical reasons);
     the typed layer wraps into `MalformedMessageError` etc. at
     `core-py/src/oigtl/runtime/exceptions.py`
   - TS: `core-ts/src/runtime/errors.ts`
   The oracle CLIs catch any exception and turn it into a JSON
   report with `ok: false, error: <message>`, so fuzzer runs
   never crash the runner.
3. **Oracle JSON report shape is stable** across all four codecs:
   `{ok, type_id, device_name, version, body_size,
   ext_header_size, metadata_count, round_trip_ok, error}`.
   The runner compares the first 7 fields; `error` text is
   expected to differ across languages.
4. **Canonical-form round-trip** is the codec contract (not
   byte-exact). All four oracles implement the same logic:
   strict length check, then accept same-length value diffs iff
   a second pack-unpack-pack pass is stable. Implemented in
   `corpus-tools/.../codec/oracle.py`,
   `core-py/src/oigtl/runtime/oracle.py::typed_verify_wire_bytes`,
   `core-cpp/src/runtime/oracle.cpp`,
   `core-ts/src/runtime/oracle.ts::verifyWireBytes`.
5. **Negative corpus** is the dual of the upstream fixtures.
   21 entries under `spec/corpus/negative/` + index.json;
   per-codec tests in each codec's test tree. Regenerate via
   `oigtl-corpus corpus generate-negative`; add new entries by
   adding a builder function in
   `corpus-tools/.../negative_corpus.py`. Entries can carry
   `known_issue` + `currently_accepted_by` annotations to track
   codec gaps (flipping from xfail to pass when fixed).
6. **No changes to wire format or schemas.** Everything in this
   harness is testing infrastructure. `spec/schemas/` is the
   contract; fuzz findings go into `spec/corpus/negative/` or
   codec fixes, never into schema changes.
7. **Phase 3 scope was revised downward** from the original
   three-language in-process fuzzer set to just C++ libFuzzer.
   See the "What's next" section above for reasoning — the
   differential runner already catches the logical-level bugs
   that Python/JS fuzzers would find, leaving memory safety
   as the only unique value-add.
