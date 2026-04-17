# security — conformance & fuzzing harness

The project's four independent codecs (reference Python, typed
Python, typed C++, typed TypeScript) give us a unique testing
setup: any disagreement between them on the same input is a bug.
The tooling here exploits that.

## What's in here

- [`PLAN.md`](PLAN.md) — phased plan for the harness. Phases 1-2
  are complete; Phases 3-5 are ongoing.
- `disagreements/*.jsonl` — fuzzer output, one disagreement per
  line. Each line records the input bytes (hex), the report from
  every oracle, and which fields disagreed. Gitignored by default;
  commit entries you want to freeze as regression data.

## Current coverage

### Phase 1 — Negative corpus *(shipped)*

21 curated must-reject inputs under
[`spec/corpus/negative/`](../spec/corpus/negative/), with a
per-codec parametrized rejection test in each of the four
implementations. Generator lives in
`corpus-tools/src/oigtl_corpus_tools/negative_corpus.py`.

Regenerate + check:

```bash
cd corpus-tools
uv run oigtl-corpus corpus generate-negative
uv run oigtl-corpus corpus generate-negative --check   # CI
```

### Phase 2 — Differential oracle fuzzer *(shipped)*

The fuzzer generates random / mutated / structured wire-byte
inputs and feeds every requested oracle the same stream. Each
oracle emits a JSON report with identical shape; the runner
compares semantic fields across reports.

Run against Python only (fast, ~22k it/s):

```bash
cd corpus-tools
uv run oigtl-corpus fuzz differential -n 100000
```

Cross-language (requires built binaries, ~5k it/s with all five oracles):

```bash
# Build the native + JS oracle CLIs first
cmake --build core-cpp/build --target oigtl_oracle_cli
(cd core-ts && npx tsc -p tsconfig.json --outDir build-tests)
(cd core-py && uv sync --all-extras)   # typed Python oracle

cd corpus-tools
uv run oigtl-corpus fuzz differential -n 100000 \
    --oracle py-ref --oracle py --oracle py-noarray \
    --oracle cpp --oracle ts
```

The five oracle options:

- `py-ref` — reference dict codec (in-process, fastest).
- `py` — typed Python classes through the numpy path
  (variable-count primitive arrays as `np.ndarray`).
- `py-noarray` — same as `py` but with ``OIGTL_NO_NUMPY=1`` forcing
  the stdlib ``array.array`` fallback path. Surfaces any divergence
  between the two coercion modes in `oigtl.runtime.arrays`.
- `cpp`, `ts` — external CLIs (`oigtl_oracle_cli` / `oracle_cli.js`).

Current 100k-iteration finding: `py`, `py-noarray`, and `py-ref`
agree bit-for-bit (0 disagreements) — the typed library faithfully
tracks the reference codec in both coercion modes. All surfaced
disagreements are cross-language (Python vs. C++ / TypeScript).

On disagreement, the runner exits non-zero and writes
`security/disagreements/<seed>.jsonl`. Inspect with:

```bash
jq -r '.disagreements + [.reports["py-ref"].error[:60]] | @tsv' \
    security/disagreements/42.jsonl | sort | uniq -c
```

#### Current state: zero disagreements

1,000,000 iterations (py-ref + py + cpp + ts) and 200,000
iterations (all five oracles including `py-noarray`) on seed 42
produce **zero disagreements**. Reject counts match exactly
across all codecs.

This wasn't the starting state — the fuzzer surfaced four real
bug classes which were all fixed in landing:

1. **ASCII strictness** (both header + body string fields) —
   unified on strict ASCII (< 0x80) across all codecs.
   Type_id / device_name / fixed_string / length_prefixed_string
   (encoding=ascii) / trailing_string (encoding=ascii) all reject
   non-ASCII at unpack. TS codegen's `TextDecoder("ascii")` is
   aliased to windows-1252 per the Encoding spec, so it was
   replaced with a strict `_readAsciiRaw` helper.
2. **Length-prefixed string bounds** — Python slicing was lenient
   (`bytes[offset:offset+length]` happily truncates). Added an
   explicit bounds check in `codec/fields.py`.
3. **Round-trip canonicalization** — Python's `struct.unpack(">f",
   ...)` and JS's `DataView.getFloat32` both route float32 values
   through the FPU which quiets signaling NaNs. C++'s memcpy path
   doesn't. The oracles now use a **canonical-form** round-trip
   check: if `pack(unpack(bytes)) != bytes`, accept if
   `pack(unpack(pack(unpack(bytes)))) == pack(unpack(bytes))`
   (codec reaches a fixed point). Strict byte-equality is kept
   for length mismatches; only value normalization is tolerated.
   Implemented identically in all four codecs.
4. **TS NUL-handling** — early bug found on the first fuzzer run;
   TS `_readAsciiNullPadded` was stripping only trailing NULs
   instead of splitting on the first NUL like Python/C++.

### Phase 3 — C++ libFuzzer memory-safety gate *(shipped)*

Two libFuzzer entry points under `core-cpp/fuzz/` run with ASan +
UBSan to catch memory-safety issues the differential runner can't
see (silent OOB reads, integer overflow, UMR). See
[`core-cpp/fuzz/README.md`](../core-cpp/fuzz/README.md) for local
build instructions.

On the first 30-second run the fuzzer found a real bug: the
`parse_wire` body-size bounds check was computing
`kHeaderSize + body_size` as `std::size_t` and wrapping when
`body_size` was near `UINT64_MAX`. A malformed 58-byte header was
enough to drive `crc64` past the end of the buffer — remote DoS.
Fixed in `core-cpp/src/runtime/oracle.cpp`; regression pinned via
`spec/corpus/negative/framing_header/body_size_uint64_max.bin`
(rejected by all four codecs).

Python (`int`) and TS (`Number(BigInt)`) codecs were not
vulnerable — arbitrary precision / IEEE-754 both side-step the
32/64-bit wrap the C++ `size_t` arithmetic fell into.

### Phase 4 — CI fuzz-smoke *(shipped)*

`.github/workflows/ci.yml` has a `fuzz-smoke` job that runs on
every PR:

1. `fuzz_header` — 60 s under ASan + UBSan on the seed corpus.
2. `fuzz_oracle` — 60 s under ASan + UBSan on the seed corpus.
3. Differential fuzzer — 50 000 iterations across all four codecs.

Any crash, sanitizer report, or cross-codec disagreement fails the
job; crash repros + disagreement logs upload as artifacts.

## Phase 5 — upstream parity fuzzer — closed

Not pursued. Functional parity with upstream is already enforced
statically via `spec/corpus/upstream-fixtures.json` (24 byte-exact
wire blobs from upstream's test vectors, round-tripped by every
codec on every PR). A live upstream oracle would surface the
reference library's own memory-safety bugs as "disagreements" —
CVE tourism, not a quality signal for us. See [`PLAN.md`](PLAN.md).
