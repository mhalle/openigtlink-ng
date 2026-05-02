# @openigtlink/core (core-ts)

Typed TypeScript wire codec for the OpenIGTLink protocol — symmetric
to [`core-cpp`](../core-cpp/) and [`core-py`](../core-py/), generated
from the same 84 schemas under [`../spec/schemas/`](../spec/schemas/).

> **Reading order:** this README is quick examples and status. For
> a guided tour of the package's three layers
> (codec / messages / net), see [`API.md`](API.md). For
> message-level questions, see
> [`../spec/MESSAGES.md`](../spec/MESSAGES.md).

## Status

**Complete.** 84 generated typed message classes, each round-trips
the upstream fixture byte-for-byte. 103 tests covering:

- hand-written runtime (byte_order, crc64, header, ext_header, oracle)
- typed round-trip for every upstream fixture
- cross-language parity against the Python oracle (skipped when `uv`
  is not on PATH — automatic in CI and in the local monorepo).

## Target environment

- **ESM only.** `"type": "module"` — no dual build.
- **Node ≥20**, modern browsers, Bun, Deno.
- **Zero runtime dependencies.** The library uses only Web platform
  primitives (`Uint8Array`, `DataView`, `BigInt`, `TextEncoder`).
  No `Buffer`, no `node:crypto`, no `node:fs`.
- Tests run under `node --test`; formatting/linting via `biome`.

## Install

```bash
npm install @openigtlink/core
```

## Usage

```ts
import { parseWire, verifyWireBytes } from "@openigtlink/core";
import { Transform, ImageMessage, Status } from "@openigtlink/core/messages";

// Construct + pack
const tx = new Transform({
  matrix: [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0],
});
const body = tx.pack();

// Parse a complete wire message (header + body)
const r = verifyWireBytes(wireBytes);
if (!r.ok) {
  console.error(r.error);
} else {
  // r.message is the typed instance; type_id is in r.header.typeId.
  if (r.header?.typeId === "TRANSFORM") {
    const m = r.message as Transform;
    console.log(m.matrix);
  }
}

// Lower-level: parse only, then dispatch by hand
const framing = parseWire(wireBytes);
if (framing.ok) {
  const tx2 = Transform.unpack(framing.contentBytes);
  console.log(tx2.matrix);
}
```

## Type mapping

| Schema type | TS type |
|---|---|
| `uint8`, `int8`, `uint16`, `int16`, `uint32`, `int32` | `number` |
| `uint64`, `int64` | `bigint` |
| `float32`, `float64` | `number` |
| `fixed_string`, `length_prefixed_string` (ascii), `trailing_string` | `string` |
| `fixed_bytes`, `length_prefixed_string` (binary) | `Uint8Array` |
| `array` of fixed-count `uint8` | `Uint8Array` |
| `array` of fixed-count non-uint8 primitive | `number[]` or `bigint[]` |
| `array` of variable-count `uint8` | `Uint8Array` |
| `array` of variable-count non-uint8 primitive | matching `TypedArray` |
| `array` of fixed_string element | `string[]` |
| `struct_array` | per-message `EntryName[]` interface |

### Why variable-count arrays decode through `DataView` (O(N) copy)

OpenIGTLink is big-endian on the wire. JavaScript typed arrays
(`Float32Array`, `Int16Array`, …) are host-endian. Reading wire
bytes directly as a typed array is silently wrong on little-endian
hosts. The runtime helpers in `runtime/byte_order.ts` decode through
`DataView` element-by-element, producing a native-endian typed
array that's correct and can be handed to WebGL / Canvas / charts
without further conversion. The per-element decode cost is low
enough that FHD images still unpack in <250 µs on Apple Silicon.

### Why 64-bit integers are `bigint`

Header `timestamp` is `uint64` (seconds since epoch + fractional
part); losing bits above 2⁵³ to `number` is a correctness bug waiting
to happen. `subcode` on STATUS and `unit` on SENSOR are likewise
`uint64`. TypeScript protobuf libraries (`@bufbuild/protobuf`,
`protobuf-ts`) follow the same convention.

## Install + test

```bash
cd core-ts
npm install
npm test         # tsc && node --test
```

## Performance

Apple Silicon, Node 25, wire bytes measured on the content body.

| Operation | time | throughput |
|---|---:|---:|
| TRANSFORM typed unpack | 0.09 µs | 482 MB/s |
| TRANSFORM typed pack | 0.53 µs | 86 MB/s |
| TRANSFORM `verifyWireBytes` (full pipeline) | 1.3 µs | 78 MB/s |
| VIDEOMETA typed unpack | 1.2 µs | 768 MB/s |
| IMAGE 50×50 `verifyWireBytes` | 9.6 µs | 262 MB/s |
| IMAGE 1920×1080 typed unpack | 154 µs | 12.8 GB/s |
| IMAGE 1920×1080 typed pack | 348 µs | 5.7 GB/s |
| IMAGE 1920×1080 `verifyWireBytes` | 4.0 ms | 490 MB/s |

Key facts:

- **FHD at 30 fps with CRC verify is comfortable.** 4 ms per full
  oracle pass leaves 29 ms of headroom per 33 ms frame.
- **CRC-64 uses split-uint32 slice-by-8.** The natural `bigint`
  polynomial arithmetic tops out at ~24 MB/s on V8. Keeping the
  running CRC in two `number` locals (hi/lo uint32) with eight
  paired HI/LO `Uint32Array` slice tables derived from the
  canonical bigint table at module load reaches **~490 MB/s** —
  ~20× over the bigint path, and faster than the `core-py` codec
  using the `crcmod` C extension. See comments in
  `src/runtime/crc64.ts` for the algebra. (This pattern doesn't
  appear to be packaged on npm; every other pure-JS CRC-64 uses
  bigint and lives at ~25 MB/s.)
- **Typed unpack is O(N) through `DataView`.** No stdlib
  `Float32Array.fromBuffer(be_dtype)` analogue exists; the per-
  element loop is what you get. 12 GB/s on FHD is fine.

Run the bench yourself:

```bash
npx tsc -p tsconfig.json --outDir build-tests
node build-tests/benches/bench_typed.js
```

## Regenerating the messages

The 84 files under `src/messages/*.ts` and `src/messages/index.ts`
are generated. To regenerate after a schema change:

```bash
uv run --project corpus-tools oigtl-corpus codegen ts
```

CI enforces drift via `oigtl-corpus codegen ts --check`.

## License

Apache 2.0. See [`../LICENSE`](../LICENSE).
