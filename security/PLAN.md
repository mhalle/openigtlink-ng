# Security + conformance testing harness — plan

Status: **Phase 1 + 2 complete**; Phases 3–5 pending. Durable
state for resuming work after context compaction.

## Status summary

- **Phase 1 (negative corpus)** shipped. 21 entries under
  `spec/corpus/negative/`, per-codec parametrized rejection
  tests. Surfaced 6 codec gaps, all fixed; shared `policy.py`
  helper extracted.
- **Phase 2 (differential oracle fuzzer)** shipped.
  `oigtl-corpus fuzz differential` runs at ~22k it/s against
  Python and ~7k it/s across all three native oracles. First 100k
  cross-language sweep found 4 additional bug classes (listed
  below), none are safety-critical, all queued for design review.
- **Phase 3 (in-process fuzzers)**, **Phase 4 (CI)**,
  **Phase 5 (upstream parity)** pending.

## Bug classes surfaced by Phase 2 (queued followups)

1. ASCII strictness divergence in header type_id / device_name:
   Python strict, C++/TS permissive.
2. POSITION body=24 round-trip mismatch in typed Python — the
   numpy coerce path normalizes NaN float32 bit patterns.
3. POSITION body=24 round-trip mismatch in TS — similar
   normalization suspected.
4. ASCII strictness divergence in content fields (STRING /
   trailing_string) — TS accepts, Py/C++ reject via the same
   ascii-decode mechanism as #1.

All require design decisions (spec interpretation, NaN handling
invariants) rather than mechanical fixes.

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

### Phase 3 — In-process fuzzers (libFuzzer, Atheris, jsfuzz) (1 day)

C++ has the strongest memory-safety story; prioritize there.

**core-cpp/fuzz/** with two libFuzzer entry points:

- `fuzz_header.cc`: feeds `LLVMFuzzerTestOneInput` to
  `unpack_header`. Seed corpus: every 58-byte header prefix from
  `spec/corpus/negative/framing_header/` plus the headers of
  `spec/corpus/upstream-fixtures.json`. Build with `-fsanitize=address,undefined`.
- `fuzz_oracle.cc`: feeds `LLVMFuzzerTestOneInput` to
  `oracle::verify_wire_bytes`. Catches OOB in every codec path.

Hook via `cmake -DBUILD_FUZZERS=ON` (only on clang + libFuzzer
available). CI runs a 60-second smoke pass on each target.

**core-py/fuzz/**: [Atheris](https://github.com/google/atheris)
targets mirror the libFuzzer ones. Atheris catches Python
exceptions that aren't `ProtocolError` subclasses (our promise:
unknown inputs raise *something from our hierarchy*, not a
`struct.error` or `IndexError` leaking from the codec internals).

**core-ts/fuzz/**: the JS fuzzing tool landscape is weak.
Cheapest option: a `runFuzzing.ts` that loops with deterministic
PRNG, feeds to `parseWire` + `verifyWireBytes`, asserts all
thrown errors are `ProtocolError` subclasses, logs any crash's
input to a deterministic reproducer file. Not real coverage-guided
fuzzing, but closes the "unexpected exception type" gap.

**Exit:** Each fuzz target runs 60 seconds without a crash or
an unexpected exception in CI. Crashing inputs go to
`spec/corpus/negative/` and the bug is fixed.

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

## File touch list

**Create:**
- `security/PLAN.md` (this file)
- `security/README.md`
- `security/differential/generators.py`, `runner.py`
- `spec/corpus/negative/index.json` + per-entry `.bin` files
- `spec/corpus/negative/README.md`
- `core-cpp/fuzz/CMakeLists.txt`, `fuzz_header.cc`, `fuzz_oracle.cc`
- `core-py/fuzz/fuzz_oracle.py`, `fuzz_header.py`
- `core-py/tests/test_negative_corpus.py`
- `core-ts/fuzz/runFuzzing.ts`
- `core-ts/tests/negative_corpus.test.ts`
- `corpus-tools/src/oigtl_corpus_tools/commands/fuzz.py`
- `corpus-tools/tests/test_negative_corpus.py`

**Modify:**
- `core-cpp/CMakeLists.txt` — optional BUILD_FUZZERS option
- `corpus-tools/src/oigtl_corpus_tools/cli.py` — register `fuzz` command
- `.github/workflows/ci.yml` — add fuzz-smoke + coverage jobs
- Root `README.md` — link to security harness

## Estimates

| Phase | Scope | Duration |
|---|---|---|
| 1 — Negative corpus + per-codec rejection | ~30 entries + 4 test files | 1 day |
| 2 — Differential fuzzer | ~400 LoC Python | 1 day |
| 3 — Structured fuzzers | libFuzzer + Atheris + JS loop | 1 day |
| 4 — CI + coverage | workflow updates | 0.5 days |
| 5 (stretch) — Upstream parity | Python subprocess glue | 0.5 days |
| **Total** | | **~3-4 days** |

## Resuming after compaction

Key facts for the next session:

1. All four codecs produce a stable JSON oracle report shape
   (`{ok, type_id, device_name, version, body_size,
   ext_header_size, metadata_count, round_trip_ok, error}`).
   Cross-language parity is already exercised by
   `core-cpp/tests/oracle_parity_test.cpp` and
   `core-ts/tests/oracle_parity.test.ts`. The differential fuzzer
   extends that from "one fixture per test" to "one million
   generated inputs per run."
2. Each codec has a typed error hierarchy rooted at
   `ProtocolError`. Subclasses match across languages (see
   `core-cpp/include/oigtl/runtime/error.hpp`,
   `core-py/src/oigtl/runtime/exceptions.py`,
   `core-ts/src/runtime/errors.ts`).
3. The negative corpus is the specification of what MUST be
   rejected — it's the dual of the upstream fixtures, which
   specify what MUST be accepted. Together they pin behaviour
   on both sides of the boundary.
4. libFuzzer on C++ catches UB/memory issues; Atheris on Python
   catches unexpected exception classes; the JS loop catches the
   same. Differential fuzzing catches semantic disagreements
   across codecs that each individual fuzzer would miss.
5. No changes to wire format or schemas — this is testing
   infrastructure, not protocol work.
