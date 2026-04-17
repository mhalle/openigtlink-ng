# upstream-oracle — conformance against the pinned reference library

Two CLIs wrapping the pinned upstream OpenIGTLink C++ reference
library (`../openigtlink-upstream`):

1. **`upstream_oracle_cli`** — consumer. Same stdin-hex → stdout-JSON
   protocol as `core-cpp/src/oracle_cli.cpp`, plugs into
   `oigtl-corpus fuzz differential` as `--oracle upstream`.
2. **`upstream_generator_cli`** — producer. Emits wire messages
   built via upstream's `Pack()` path (one lowercase hex line per
   stdout line). Drives `oigtl-corpus fuzz roundtrip`.

The two directions catch different bug classes:

- The consumer oracle catches divergences where *our pack* emits
  bytes upstream accepts — any cross-oracle disagreement on semantic
  fields for the same input is a bug.
- The producer oracle catches divergences where *upstream emits*
  bytes one of our codecs can't handle — any reject or round-trip
  mismatch on upstream-generated input is a bug in our codec.

## Scope

**Functional-parity oracle, not a fuzz target.** The runner gates
invocation: upstream only sees inputs that at least one other
oracle already accepted. That keeps upstream's unhardened readers
away from the adversarial mutation stream where they'd crash
constantly and drown the signal. When upstream *does* crash on a
well-formed-enough input to slip past the gate, the runner catches
the subprocess death, logs `error="upstream crashed"`, respawns,
and continues — those crashes are filed as upstream bugs we don't
own.

The `round_trip_ok` semantic field is excluded when comparing
against upstream: upstream canonicalises trailing padding bytes
(e.g. non-NUL bytes in the type_id / device_name null-padded
regions) on `Pack()`, whereas our four codecs preserve the input
byte-for-byte. Both behaviours are spec-conformant; the
canonicalisation just drifts them apart on mutated inputs.

## Build

Upstream must be built first:

```bash
cmake -S corpus-tools/reference-libs/openigtlink-upstream \
      -B corpus-tools/reference-libs/openigtlink-upstream/build \
      -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF -DBUILD_SHARED_LIBS=OFF
cmake --build corpus-tools/reference-libs/openigtlink-upstream/build --parallel
```

Then this target:

```bash
cmake -S corpus-tools/reference-libs/upstream-oracle \
      -B corpus-tools/reference-libs/upstream-oracle/build
cmake --build corpus-tools/reference-libs/upstream-oracle/build --parallel
```

## Run

Consumer oracle (cross-codec comparison on fuzzer-generated inputs):

```bash
cd corpus-tools
uv run oigtl-corpus fuzz differential -n 100000 \
    --oracle py-ref --oracle cpp --oracle ts --oracle upstream \
    --progress-every 25000
```

Producer oracle (roundtrip valid upstream-emitted vectors through
our codecs):

```bash
cd corpus-tools
uv run oigtl-corpus fuzz roundtrip -n 10000 -s 1 \
    --oracle py-ref --oracle cpp --oracle ts

# Or restrict to one upstream type:
uv run oigtl-corpus fuzz roundtrip -n 10000 --type IMAGE \
    --oracle py-ref --oracle cpp --oracle ts
```

Supported generator types: `TRANSFORM`, `POSITION`, `STATUS`,
`STRING`, `SENSOR`, `POINT`, `IMAGE`. Round-robins all seven when
`--type` is omitted.

## Notes on upstream coverage

Upstream's default `MessageFactory` omits classes it ships but
doesn't auto-register (BIND / COLORT / NDARRAY / SENSOR). We
register them explicitly in `oracle_cli.cpp`'s factory init. Two
types upstream does not support at all:

- **COLORTABLE** — upstream only knows the 6-char `COLORT` name for
  the same wire format. Inputs with `type_id=COLORTABLE` report
  `ok=true, round_trip_ok=false, error="upstream has no codec..."`.
- **VIDEOMETA** — upstream only ships `VIDEO` under an `#ifdef`.
  Same no-codec report.

Both are filtered out of the cross-oracle comparison (see
`fuzz/runner.py::_compare`).
