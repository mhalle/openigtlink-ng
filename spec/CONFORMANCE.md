# Conformance testing

This document explains how correctness is verified in
openigtlink-ng — what tests exist, what each one proves, how
they're connected, and how to run them locally.

The short version: four independent codecs (reference Python,
typed Python, typed C++, typed TypeScript) are held byte-
identical by a multi-layered test suite. Any disagreement
between any two of them on any input is a bug and fails CI.

For the security-specific perspective on all of this (fuzzer
internals, bugs found, what's guaranteed), see
[`SECURITY.md`](/SECURITY.md) and
[`security/README.md`](/security/README.md). This document
focuses on the day-to-day mechanics.

---

## Layers of test coverage

Four distinct layers, in increasing order of what they catch:

### 1. Per-core unit tests

Every core has a `tests/` directory running the language's
native test runner. These cover the typed API surface, codec
edge cases specific to the language's type system, and any
language-specific behavior (e.g., numpy array coercion in
Python, `DataView` offset handling in TypeScript, ASIO error
mapping in C++).

Run them from each core's directory; commands are in
[`CONTRIBUTING.md`](/CONTRIBUTING.md).

### 2. Upstream fixtures (positive corpus)

Twenty-four byte-exact wire blobs lifted from the canonical
[upstream OpenIGTLink](https://github.com/openigtlink/OpenIGTLink)
test vectors live under [`corpus/upstream-fixtures.json`](corpus/upstream-fixtures.json).
Every codec round-trips them: decode → re-encode → assert
byte-identical with the original. A single miss anywhere in
any core fails CI.

The fixtures are frozen; they change only when upstream ships
new ones and we pick them up deliberately. The pinned upstream
commit is recorded in
[`corpus/ORACLE_VERSION.md`](corpus/ORACLE_VERSION.md).

### 3. Negative corpus (must-reject)

Twenty-one adversarial inputs under
[`corpus/negative/`](corpus/negative/) — malformed framing,
length overflows, encoding violations, integer-wrap hazards.
Every codec must reject every entry. A codec that silently
accepts a negative entry fails CI.

Entries in the negative corpus are not arbitrary; each one
pins a regression for a specific bug class the fuzzer (or a
manual audit) surfaced. Adding one is how we make sure the
same bug can't come back.

Each entry is a `.bin` file plus an adjacent `.json`
describing why it must be rejected. See
[`corpus/negative/README.md`](corpus/negative/README.md).

### 4. Cross-language differential fuzzer

The heaviest layer. Described in detail in
[`security/README.md`](/security/README.md); summary here:

- Generates randomized / mutated / structured wire bytes.
- Feeds each input to every configured codec oracle (four
  codecs + variants, run as in-process calls where possible
  and subprocess CLIs where not).
- Each oracle emits a JSON report with identical shape;
  the runner compares semantic fields.
- Any disagreement fails the run and writes the input plus
  all reports to `security/disagreements/` for debugging.

Run on every PR (50k iterations across all four codecs) and
weekly in a background pipeline (1M iterations). Zero
disagreements at both scales as of writing.

### 5. Cross-runtime interop tests

Per-pair end-to-end tests that spin up a real server in one
language and a real client in another, speaking over real
sockets. Not mocks.

Tests live at `core-*/tests/cross_runtime_*`; the pattern is:

- A "fixture" (e.g., `ts_tcp_echo.ts`, `python_tcp_echo.py`,
  `cpp_tcp_echo.cpp`) stands up a minimal echo server in that
  language, binds to a random port, prints `PORT=<n>\n` to
  stdout, and echoes TRANSFORM messages back as STATUS.
- The peer-language test spawns the fixture as a subprocess,
  reads the port, opens a real TCP (or WebSocket) connection,
  round-trips a message, asserts the reply content matches.

This catches problems the byte-level fuzzer can't see:
connection lifecycle bugs, framing reassembly on real streams
(which break frames across send boundaries differently than
in-memory byte arrays do), interop-specific header-version
handshake, backpressure behavior.

---

## What the layers prove, collectively

- **Every codec decodes what upstream encodes.** (Layer 2 —
  upstream fixtures.)
- **Every codec rejects what we've agreed is malformed.**
  (Layer 3 — negative corpus.)
- **Every codec decodes + re-encodes the same bytes.** (Layer
  2 again, by round-trip; plus the canonical-form rule for
  float-NaN edge cases — see
  [`security/README.md`](/security/README.md).)
- **No two codecs disagree on any randomized input.** (Layer
  4 — differential fuzzer.)
- **Real clients talk to real servers across languages.**
  (Layer 5 — cross-runtime interop.)

If all five layers pass, the following properties hold to the
level the test suite can verify:

- **Wire-level compatibility** with upstream OpenIGTLink on
  every message type in the spec.
- **Cross-language parity** on the complete fuzzer-explored
  input space.
- **No silent acceptance** of inputs in the negative corpus.
- **Live network interop** between every language pair that
  shares a transport.

---

## Running the suite locally

### The fast path (what CI runs on every PR)

```bash
# Per-core unit tests — run each core's native command
cd core-py && uv run pytest
cd ../core-ts && npm test
cmake -S core-cpp -B core-cpp/build && cmake --build core-cpp/build
ctest --test-dir core-cpp/build

# Differential fuzzer — 50k iterations, ~10 seconds
cd corpus-tools
uv run oigtl-corpus fuzz differential -n 50000 \
    --oracle py-ref --oracle py --oracle cpp --oracle ts

# libFuzzer smoke on C++ — 60 seconds each
cmake --build core-cpp/build-fuzz --target fuzz_header fuzz_oracle
core-cpp/build-fuzz/fuzz_header -max_total_time=60
core-cpp/build-fuzz/fuzz_oracle -max_total_time=60
```

### Before pushing a codec or framing change

Run the longer-form fuzzer locally:

```bash
cd corpus-tools
uv run oigtl-corpus fuzz differential -n 1000000 \
    --oracle py-ref --oracle py --oracle cpp --oracle ts
```

~3 minutes on modern hardware. Catches rare disagreements the
50k-iteration CI smoke might miss.

### Before pushing a transport change

The cross-runtime interop tests are part of each core's normal
test run, but they self-skip if the other languages' binaries
aren't built. For a full interop check:

```bash
# Build core-ts fixtures
cd core-ts && npx tsc -p tsconfig.json --outDir build-tests

# Build core-cpp fixtures + tests
cmake --build core-cpp/build --target cpp_tcp_echo

# Now run cross-runtime tests anywhere
cd core-py && uv run pytest -k cross_runtime
cd ../core-ts && npm test -- --test-only cross_runtime
cd ../core-cpp/build && ctest -R cross_runtime
```

---

## Adding tests

**For a new message type:** the positive corpus gets a round-
trip entry; the per-core test suites pick it up automatically
via parametrized runs over the corpus.

**For a newly-found bug:** pin a negative corpus entry if the
bug is about rejection semantics. If the bug is about
cross-language agreement, the differential fuzzer's
reproducing seed goes under `security/disagreements/` as a
committed regression. If the bug is about a specific code
path that fuzzing didn't reach, add a targeted test in the
affected core's `tests/`.

**For a new transport:** add a fixture in the new core's
`tests/fixtures/` that mirrors the existing `_tcp_echo` /
`_ws_echo` pattern, then peer-language tests that spawn it.

See [`CONTRIBUTING.md`](/CONTRIBUTING.md) for the testing-
burden table by change type.

---

## Why the oracle is the reference Python codec

A quick note on why we built a deliberately un-optimized
dict-based codec and then hold three production codecs against
it: the reference codec is designed to be *obviously correct*.
It's slow. It allocates more than it has to. It has no clever
bit tricks. Its job is to be the shortest path from a schema
to "did this byte sequence parse correctly?"

When the fuzzer finds a disagreement, the first question is
"what does the reference say?" If the production codec
disagrees with the reference, we investigate the production
codec. If the reference itself disagrees with upstream
fixtures or the spec, *that's* the interesting case and
triggers a spec conversation.

This layering has been load-bearing. Four fuzzer-discovered
bug classes (documented in
[`security/README.md`](/security/README.md)) were resolved
fastest by asking the reference codec the same question; in
every case it agreed with upstream and the production codec
was wrong.
