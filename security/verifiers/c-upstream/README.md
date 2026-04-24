# c-upstream verifier

An independent low-level differential-check participant —
upstream OpenIGTLink's pure-C byte layer (`Source/igtlutil/`),
compiled directly into a CLI with no dependency on upstream's
CMake configure step.

## Terminology: verifier vs. oracle

This is a *verifier*, not an oracle. In strict testing
terminology:

- **Oracle** — the authoritative reference that resolves
  "which implementation is right?" For this project that's
  [`corpus-tools/src/oigtl_corpus_tools/codec/`](../../../corpus-tools/src/oigtl_corpus_tools/codec/)
  (the `py-ref` reference codec) backed by the upstream test
  fixtures in [`spec/corpus/`](../../../spec/corpus/).
- **Verifier** — an independent implementation plugged into
  the differential runner. Verifiers should agree with the
  oracle and with each other on spec-conformant input; when
  they don't, the oracle decides who's right.

The differential fuzzer's CLI flag is historically named
`--oracle`, and its choices list includes `py-ref`, `py`,
`cpp`, `ts`, `upstream`, and this new `c-upstream`. In that
list only `py-ref` is an oracle in the strict sense; the
others are verifiers. The flag's looseness is a pre-existing
thing the code calls out inline but doesn't untangle.

## Why this verifier

Upstream's C++ `igtl::` classes call into its C layer
(`igtlutil/igtl_*.c`) for all byte-level work: endian
conversion, CRC computation, fixed-header serialization. The
C layer is therefore **one step closer to the wire** than the
C++ layer — it sees exactly the bytes that go out, without any
C++ abstraction in between.

Adding it as a separate differential participant (distinct
from the existing `upstream` entry, which wraps the C++ API)
gives us:

- A different level of abstraction (raw C, not C++) —
  catches cases where upstream's C++ layer happens to
  normalise something its own C layer accepts or vice versa.
- Minimal build complexity: four `.c` files plus a minimal
  `igtlConfigure.h` stand-in. No upstream CMake configure
  required.

## Scope — round 1

Supports:

- **TRANSFORM** (v1, 48-byte body)
- **STATUS** (v1, variable-size body)

Declines (emits `ok=false` with a descriptive error):

- **Unsupported type_ids.** The other 20 message types
  upstream's C layer supports are out of scope for this
  verifier until we broaden it.
- **v2 / v3 framing.** Upstream's C layer has no
  extended-header / metadata codec — that lives in C++.
  Any input with `header_version >= 2` is declined.

The differential runner's comparison logic filters
`c-upstream`'s declined reports out of the cross-comparison,
so out-of-scope inputs are simply skipped for this
participant rather than treated as disagreements.

## Known divergences from our codecs

Running this verifier alongside `py-ref` surfaces a specific
class of divergence we already know about: **upstream's C
layer does not validate that ASCII-declared string fields are
actually ASCII.** Specifically:

- STATUS `error_name` (20-byte fixed-string): upstream's C
  layer accepts any bytes; all four of our codecs reject
  non-ASCII at unpack time (per the v3 spec).
- Similar lenience at every other ASCII-declared string field
  upstream's C layer touches.

These are **real findings** about upstream's permissiveness,
not bugs in us. See `security/README.md` Phase 2 §"ASCII
strictness" for the decision our codecs made. The differential
runner currently surfaces these as disagreements; if you want
to filter them out for a clean CI gate, extend the runner's
`_compare()` function with an "upstream C is permissive on
ASCII" skip rule.

In other words: the oracle (py-ref) is right. The c-upstream
verifier reveals where upstream's C code deviates from the
oracle — which is precisely the job we want a verifier to do.

## Building

The build target is gated on the upstream clone being present
at `corpus-tools/reference-libs/openigtlink-upstream/`. If the
clone is there, CMake configures the `oigtl_c_upstream_static`
library (4 upstream source files) and the
`oigtl_c_upstream_verifier_cli` executable automatically:

```bash
cmake -S core-cpp -B core-cpp/build
cmake --build core-cpp/build --target oigtl_c_upstream_verifier_cli
```

The upstream clone is pinned via
[`spec/corpus/ORACLE_VERSION.md`](../../../spec/corpus/ORACLE_VERSION.md);
CI fetches it before configuring.

## Running directly

```bash
echo "<hex-wire-bytes>" | core-cpp/build/oigtl_c_upstream_verifier_cli
# → { "ok": ..., "type_id": "...", "device_name": "...", ... }
```

One hex-encoded input per line, one JSON report per output
line. Same protocol as `core-cpp/src/oracle_cli.cpp` and the
other differential-check CLIs.

## Running under the differential fuzzer

```bash
cd corpus-tools
uv run oigtl-corpus fuzz differential -n 100000 \
    --oracle py-ref --oracle c-upstream
```

Combine with other participants at will. `c-upstream` is not
gated (unlike the full C++ `upstream` verifier), so it runs
on every input the fuzzer produces.

## Not in this increment

- **Full type coverage.** Extending from 2 types to all 22 is
  a mechanical expansion (one switch case per type, plus
  library link targets in CMake). Deferred until this MVP
  has demonstrated the approach works under sustained use.
- **CI gating.** This verifier is opt-in; adding it to the CI
  smoke would require handling the ASCII-lenience divergences
  documented above. Worthwhile when a broader set of filters
  is decided on.
- **v2/v3 support.** Impossible at the C layer — would need
  upstream's C++ layer, which is what the existing `upstream`
  participant provides.

## File layout

```
security/verifiers/c-upstream/
├── README.md             (this file)
├── verifier_cli.c        (the CLI source)
├── igtlConfigure.h       (stand-in for upstream's generated one)
└── igtl_typeconfig.h     (stand-in for upstream's generated one)
```

CMake wiring lives in `core-cpp/CMakeLists.txt` under the
`IGTL_UPSTREAM_ROOT` section.
