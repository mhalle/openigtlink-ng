# core-ts — plan

Status: **complete (Phases 1-6)**.

Summary of what shipped:

- `oigtl-corpus fixtures export-json` subcommand + committed
  `spec/corpus/upstream-fixtures.json` (24 fixtures).
- `core-ts/src/runtime/` — byte_order, crc64, errors, header,
  ext_header, dispatch, oracle. Pure Web-platform primitives.
- `corpus-tools/.../codegen/ts_{types,emit}.py` + two Jinja
  templates; new `oigtl-corpus codegen ts [--check]` CLI.
- 84 generated message modules + `index.ts` registry populator.
- 103 tests pass (runtime + all 24 fixtures round-trip + 24
  cross-language oracle parity vs. Python).
- CI matrix adds a `typescript` job on Node 20 + 22, plus drift
  guards for TS codegen and the fixtures JSON.
- Bench numbers documented in `core-ts/README.md`.

Historical plan retained below.

---

## Goal

Ship a TypeScript wire codec for the OpenIGTLink protocol, symmetric
to [`core-cpp`](../core-cpp/) and [`core-py`](../core-py/). Same 84
messages, same conformance corpus, generated from the same schemas
under [`../spec/schemas/`](../spec/schemas/).

Acceptance criterion: **a browser-based viewer can parse a live
1920×1080 IMAGE stream at 30 fps** and a Node-side adapter can
relay TRANSFORM + IMAGE traffic between a tracking device and a
browser, with every upstream fixture round-tripping byte-for-byte.

## Target environments

- **ESM only.** `"type": "module"` in package.json; no dual build.
  Node ≥20, modern browsers (ES2022), Bun, Deno.
- **Web-compatible runtime.** `Uint8Array` + `DataView` + `BigInt`;
  no `Buffer`, no `node:crypto`, no `node:fs` in the library itself.
  (Tests may use `node:test` + `node:fs` to load fixtures.)
- **Zero runtime dependencies.** All primitives are Web-standard. The
  library ships typed API + codegen output and nothing else.

## Design decisions

These came out of the chat thread that produced this plan. Writing
them down so we don't relitigate after compaction.

### 1. `bigint` for 64-bit integers, `number` for ≤32-bit

Headers carry `uint64 timestamp`. STATUS has `uint64 subcode`.
SENSOR has `uint64 unit`. Representing them as `number` silently
loses precision above 2⁵³. The modern TypeScript protobuf crates
(`protobuf-ts`, `@bufbuild/protobuf`) all use `bigint` for 64-bit
fields — users aren't surprised.

Downside: `bigint` doesn't mix with `number` in arithmetic without
explicit `BigInt()`/`Number()`. Users who only care about the
32-bit-sec + 32-bit-frac timestamp will access `.secondsSince1970`
/ `.fractionOfSecond` helpers rather than the raw `bigint`.

### 2. Host-endian typed arrays via DataView decode (copy at codec boundary)

Protocol is big-endian. `Float32Array` and friends are host-endian.
Reading wire bytes directly as `new Float32Array(buffer)` silently
breaks on little-endian hosts (i.e. all real hardware).

Two options considered:

- **(a) DataView decode → native-endian TypedArray.** One copy at
  the codec boundary. User receives an ordinary `Float32Array`
  they can pass to WebGL / Canvas / charts without thinking
  about endianness.
- **(b) Zero-copy `Uint8Array` + helper getters.** Matches the
  numpy-with-BE-dtype approach in core-py. More memory-efficient
  for read-once-discard workloads but surprises every downstream
  API that expects native-endian numbers.

**Choice: (a).** The copy is O(N) but runs at ~1-2 GB/s on modern
CPUs and happens at decode time where we're already walking bytes.
Predictable, ergonomic, matches Web platform norms. Power users
who need zero-copy can skip the codec and DataView the raw frame.

### 3. Hand-rolled validation (no zod / valibot)

Per-message validation is structurally simple — length checks,
enum range checks, numeric bounds. Codegen can emit those
imperatively in each generated class's `unpack()` method. Zero
runtime dependency, zero bundler concerns, and it matches the
core-cpp approach.

`zod`/`valibot` would buy composable schemas that the user could
extend. For a wire protocol whose schema is fixed by the spec,
that's the wrong shape of flexibility.

### 4. ESM-only, no dual build

It's 2026. Node has been ESM-native since 2021. Every modern
packager (Vite, Bun, webpack 5+) resolves `"type": "module"`
cleanly. Shipping CJS would carry water for a small minority of
stale toolchains and add 30-40% to the package size.

### 5. `tsc` alone, no bundler for the library

`tsc` produces `.js` + `.d.ts` + source maps. Consumers bundle if
they want to. Tree-shaking works cleanly with per-message deep
imports (`@openigtlink/core/messages/transform`) via an `exports`
map.

### 6. `node --test` for the test suite

Stdlib test runner shipped in Node ≥18. No `jest`, `vitest`,
`mocha` dep. Test files end in `.test.ts`, compiled to
`.test.js`, invoked via `node --test dist/**/*.test.js`. Matches
the zero-runtime-dep ethos.

### 7. `biome` for format + lint

One tool, one config, written in Rust. Replaces `eslint` +
`prettier` + typescript-eslint. Fast enough to run on every save
and in CI.

### 8. Shared fixtures via JSON export from corpus-tools

The C++ and Python sides read upstream `.h` files via the Python
extractor in `corpus-tools`. TS can't easily call that. Rather
than port the extractor, we add a new subcommand:

```
uv run oigtl-corpus fixtures export-json > spec/corpus/upstream-fixtures.json
```

Output is `{ name: hex_string }` (or `{ name: { bytes, parts } }`
for multi-region fixtures). The JSON file is committed. TS tests
load it via `fs.readFileSync` in the Node test harness.

Reuse side-effect: this file is also useful for browser-based
demos that want canned fixtures without running the extractor.

## Architecture

```
┌────────────────────────────────────────────────────────────┐
│  User-facing typed message classes                         │
│    @openigtlink/core/messages/transform → Transform class │
│    .pack(): Uint8Array                                     │
│    static unpack(bytes: Uint8Array): Transform             │
├────────────────────────────────────────────────────────────┤
│  Generated codec walkers (per message)                     │
│    Emit the field walk inline — no generic runtime         │
│    dispatch. Same shape as core-cpp generated .cpp.        │
├────────────────────────────────────────────────────────────┤
│  Hand-written runtime (src/runtime/)                       │
│    byte_order.ts    DataView BE read/write helpers        │
│    crc64.ts         CRC-64 ECMA-182 slice-by-8            │
│    header.ts        58-byte header pack/unpack            │
│    ext_header.ts    v3 extended header + metadata         │
│    dispatch.ts      type_id → class registry              │
│    errors.ts        ProtocolError hierarchy               │
│    oracle.ts        verifyWireBytes() parity wrapper      │
└────────────────────────────────────────────────────────────┘
```

### Type mapping

| Schema type | TS type |
|---|---|
| `uint8`, `int8`, `uint16`, `int16`, `uint32`, `int32` | `number` |
| `uint64`, `int64` | `bigint` |
| `float32`, `float64` | `number` |
| `fixed_string` | `string` |
| `fixed_bytes` | `Uint8Array` |
| `length_prefixed_string` (ascii) | `string` |
| `length_prefixed_string` (binary) | `Uint8Array` |
| `trailing_string` | `string` |
| `array` of fixed-count `uint8` | `Uint8Array` |
| `array` of fixed-count non-uint8 primitive | `number[]` (or `bigint[]` for 64-bit ints) |
| `array` of variable-count `uint8` | `Uint8Array` |
| `array` of variable-count `int16` | `Int16Array` |
| `array` of variable-count `uint16` | `Uint16Array` |
| `array` of variable-count `int32` | `Int32Array` |
| `array` of variable-count `uint32` | `Uint32Array` |
| `array` of variable-count `int64` | `BigInt64Array` |
| `array` of variable-count `uint64` | `BigUint64Array` |
| `array` of variable-count `float32` | `Float32Array` |
| `array` of variable-count `float64` | `Float64Array` |
| `struct_array` | `NestedType[]` |

Fixed-count arrays stay as plain arrays for the same reason as
core-py: they're always ≤12 elements and TypedArray allocation
overhead dominates at that size.

## Phased work

### Phase 1 — Fixtures export subcommand (0.5 days)

In `corpus-tools/src/oigtl_corpus_tools/commands/`, add a new
`fixtures.py` with one subcommand:

```
oigtl-corpus fixtures export-json [--output PATH]
```

Dumps `UPSTREAM_VECTORS` as a JSON file mapping fixture name →
`{ hex: "...", type_id: "...", body_size: N }`. Also handles
multi-region fixtures (BIND, POLYDATA, NDARRAY) by serializing
the concat result.

Commit the output at `spec/corpus/upstream-fixtures.json` (or
similar). A CI drift check regenerates and compares.

**Exit:** JSON file exists and contains all 24 fixtures; TS tests
can load it cold.

### Phase 2 — Hand-written runtime (~800 LoC, 1.5 days)

`core-ts/src/runtime/`:

- `byte_order.ts` — big-endian `readU8/U16/U32` + `readBigU64`,
  `readI*`, `readF32/F64`, plus matching writers. Thin wrappers
  over `DataView`. Everything typed and inlined by TS.
- `crc64.ts` — CRC-64 ECMA-182 slice-by-8, port of the core-cpp
  `make_tables()` at compile time (literal table constants baked
  in). Works on `Uint8Array` inputs. Returns `bigint`.
- `header.ts` — `packHeader(opts) -> Uint8Array(58)`,
  `unpackHeader(view: DataView): Header`. `Header` type is fully
  typed, including `bigint` timestamp.
- `ext_header.ts` — v3 extended header + metadata region codec.
- `errors.ts` — `ProtocolError` → `HeaderParseError`,
  `CRCMismatchError`, `BodyDecodeError`, etc.
- `oracle.ts` — `verifyWireBytes(bytes): VerifyResult` matching
  the C++ and Python oracle shape (for cross-language parity CI).

Unit tests cover CRC-64 (ECMA test vector), header round-trip,
extended-header round-trip.

**Exit:** ~40 runtime tests pass; `verifyWireBytes` produces the
same JSON as the C++ and Python oracles for the TRANSFORM fixture.

### Phase 3 — Codegen for TypeScript (~400 LoC codegen, 1.5 days)

In `corpus-tools/src/oigtl_corpus_tools/codegen/`:

- `ts_types.py` — schema-type → TS-type mapping (the big table above).
- `ts_emit.py` — Jinja-based emitter. Two templates:
  - `ts_message.ts.jinja` — one per message, emits an exported
    interface + class with hand-rolled `pack()`/`unpack()`.
  - `ts_index.ts.jinja` — re-export + registry.
- New CLI: `oigtl-corpus codegen ts [--check]`.

Output: `core-ts/src/messages/transform.ts` etc., 84 modules +
`index.ts`. Every file carries the `GENERATED` banner; drift
check runs in CI.

Codegen walks each field directly, emitting imperative code:

```ts
// TRANSFORM
static unpack(bytes: Uint8Array): Transform {
  if (bytes.length !== 48) throw new BodyDecodeError(...);
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const matrix = new Array<number>(12);
  for (let i = 0; i < 12; i++) matrix[i] = view.getFloat32(i * 4, false);
  return new Transform({ matrix });
}
```

For variable-count `float32` fields the unpack looks like:

```ts
const out = new Float32Array(count);
for (let i = 0; i < count; i++) out[i] = view.getFloat32(offset + i * 4, false);
// or, faster path for multi-MB:
//   Copy into a new ArrayBuffer, then byteswap() via a pass over DataView.
```

Pack is the symmetric pattern via `view.setFloat32(..., false)`.

**Exit:** all 84 modules regenerate; `oigtl-corpus codegen ts --check`
green; every generated file compiles under `tsc --strict`.

### Phase 4 — Tests: upstream-fixture round-trip (0.5 days)

Mirror of `core-py/tests/test_round_trip_all.py`:

- Load `spec/corpus/upstream-fixtures.json` once.
- For each fixture, decode via the typed registry, re-encode, and
  assert byte-equal.
- Separate `oracle_parity.test.ts` that runs the Python oracle
  via `child_process.execFileSync` and compares JSON outputs.

**Exit:** 23/24 upstream fixtures round-trip in TS (rtpwrapper
excluded, matching the other two languages). CI matrix runs
`node --test` on Node 20 + 22.

### Phase 5 — Packaging + benchmark (0.5 days)

`package.json`:

```json
{
  "name": "@openigtlink/core",
  "version": "0.1.0",
  "type": "module",
  "engines": { "node": ">=20" },
  "exports": {
    ".": { "import": "./dist/index.js", "types": "./dist/index.d.ts" },
    "./messages/*": { "import": "./dist/messages/*.js", "types": "./dist/messages/*.d.ts" },
    "./runtime/*": { "import": "./dist/runtime/*.js", "types": "./dist/runtime/*.d.ts" }
  },
  "files": ["dist", "README.md", "LICENSE"]
}
```

Build: `tsc -p tsconfig.build.json` → `dist/`. Dev: `tsc --watch`.
Tests: `node --test 'dist/**/*.test.js'`. Lint/format: `biome check`.

Bench: `benches/bench-typed.ts` mirroring `core-py/benches/bench_typed.py`.
Same fixtures (TRANSFORM / IMAGE / VIDEOMETA) + synthetic VGA/XGA/FHD.
Report µs/op and MB/s.

Expected numbers on Node 22 / Apple Silicon (to validate against):

| Operation | target |
|---|---|
| TRANSFORM unpack | ≤ 2 µs |
| IMAGE 50×50 unpack | ≤ 10 µs |
| IMAGE 1920×1080 unpack | ≤ 5 ms (CRC dominant) |
| TRANSFORM `parseMessage` (full pipeline) | ≤ 5 µs |

If numbers come in 2-3× worse than core-py typed, investigate
before declaring Phase 5 done. The two codecs are doing the same
work; meaningful gaps indicate a codegen inefficiency.

**Exit:** `npm publish --dry-run` produces a clean tarball;
bench numbers documented in README; CI publishes on tag.

### Phase 6 — CI + docs (0.5 days)

Add a `ts` job to `.github/workflows/ci.yml`:

- Checkout, set up Node 20 and 22.
- `npm ci && npm run build && npm test`.
- `uv run oigtl-corpus codegen ts --check` (drift guard).
- `biome check`.

Root `README.md` learns a core-ts row in the status table.
`core-ts/README.md` follows the format of `core-py/README.md` —
install, usage, performance, dependency notes.

**Exit:** CI green on main; root README reflects complete state.

## File touch list

**Create:**
- `core-ts/PLAN.md` (this file — already drafted)
- `core-ts/package.json`, `tsconfig.json`, `tsconfig.build.json`, `biome.json`
- `core-ts/src/runtime/{byte_order,crc64,header,ext_header,errors,oracle,dispatch}.ts`
- `core-ts/src/messages/*.ts` — 84 generated files
- `core-ts/src/messages/index.ts` — generated re-exports + registry
- `core-ts/tests/runtime/*.test.ts` — hand-written runtime tests
- `core-ts/tests/round_trip_all.test.ts` — parametrized fixture round-trip
- `core-ts/tests/oracle_parity.test.ts` — cross-language parity
- `core-ts/benches/bench-typed.ts`
- `core-ts/README.md`
- `corpus-tools/src/oigtl_corpus_tools/commands/fixtures.py`
- `corpus-tools/src/oigtl_corpus_tools/codegen/ts_types.py`
- `corpus-tools/src/oigtl_corpus_tools/codegen/ts_emit.py`
- `corpus-tools/src/oigtl_corpus_tools/codegen/templates/ts_message.ts.jinja`
- `corpus-tools/src/oigtl_corpus_tools/codegen/templates/ts_index.ts.jinja`
- `spec/corpus/upstream-fixtures.json` (generated, committed)

**Modify:**
- `corpus-tools/src/oigtl_corpus_tools/cli.py` — register `fixtures`
  command and `codegen ts` subcommand.
- `.github/workflows/ci.yml` — add `ts` job + Node matrix.
- Root `README.md` — status table.
- `corpus-tools/README.md` — document new subcommands.

**Do not touch:**
- Any `core-cpp/` or `core-py/` file — wire contract unchanged.
- `spec/schemas/*.json` — schema contract unchanged.
- `corpus-tools/src/oigtl_corpus_tools/schema/` — Pydantic meta-schema
  is target-independent.

## Estimates

| Phase | Scope | Duration |
|---|---|---|
| 1 — Fixtures export | 150 LoC Python + JSON file | 0.5 days |
| 2 — Hand-written runtime | ~800 LoC TS + 40 tests | 1.5 days |
| 3 — Codegen | ~400 LoC Python + 2 Jinja templates | 1.5 days |
| 4 — Round-trip tests + parity | ~200 LoC TS | 0.5 days |
| 5 — Packaging + bench | ~300 LoC TS + docs | 0.5 days |
| 6 — CI + docs | config + README | 0.5 days |
| **Total** | **~1650 LoC + tests + docs** | **~5 days** |

Similar to core-py's scope (which came in at ~4 days of work). TS
adds packaging/CI complexity but removes the numpy-extra branching
that ate half of Phase 2 in core-py.

## Open questions

- **BigInt performance on very large integer arrays.** `BigInt64Array`
  is slower to iterate than `Int32Array`; for hypothetical future
  int64 payload messages (none exist today) we may want to expose
  a `.asInt32Pairs()` helper. Not a blocker.
- **WebAssembly CRC-64.** Pure-JS slice-by-8 gets ~500 MB/s. WASM
  could hit 2-3× that. Defer to Phase 5 bench results — if
  `parseMessage` on FHD IMAGE is within 2× of the core-py+crcmod
  number, pure JS is fine.
- **Streaming parser.** Not in this plan. Today's codecs all take
  a complete `Uint8Array` body. A `ReadableStream<Uint8Array>`
  adapter would be a nice sibling module but belongs in a
  hypothetical `transport-ts` package alongside WebSocket / TCP
  adapters.

## Resuming after compaction

Key facts for the next session:

1. `corpus-tools` already has C++ and Python codegen. The TS
   emitter mirrors the Python one structurally — start by reading
   `corpus-tools/src/oigtl_corpus_tools/codegen/python_{types,emit}.py`
   and the `python_message.py.jinja` template.
2. The wire protocol is big-endian. TypedArrays are host-endian.
   `DataView` is the only safe reader. Never `new Float32Array(buffer)`
   straight from the wire.
3. `bigint` is required for `uint64`/`int64`. Headers carry a
   `uint64` timestamp, so Header.timestamp is `bigint` from day one.
4. Phases are sequential and each individually shippable.
   Phase 1 (fixtures export) blocks Phase 4 (tests), but nothing
   else.
5. Target ESM-only, Node ≥20, zero runtime deps, `tsc` alone for
   build, `node --test` for tests, `biome` for format/lint.
6. If confused about any design decision, read this file's
   "Design decisions" section — each one captures a specific
   tradeoff considered and rejected.
