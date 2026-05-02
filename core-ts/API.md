# `@openigtlink/core` API tour

This is a guided tour of the `@openigtlink/core` TypeScript package
— what's in it, how the pieces relate, when to use which. It sits
between the quick examples in [`README.md`](README.md) and per-symbol
JSDoc in the source.

For message-level questions ("what fields does TRANSFORM have?"),
see [`../spec/MESSAGES.md`](../spec/MESSAGES.md).

## Three concentric layers

```
┌──────────────────────────────────────────────────────┐
│  @openigtlink/core/net                               │
│  TCP + WebSocket clients and servers, framing,       │
│  resilient connections. Node + browser + Bun + Deno. │
└────────────────────┬─────────────────────────────────┘
                     │ delegates to
┌────────────────────┴─────────────────────────────────┐
│  @openigtlink/core/messages                          │
│  84 built-in typed message classes + an extension    │
│  registration API.                                   │
└────────────────────┬─────────────────────────────────┘
                     │ encodes/decodes via
┌────────────────────┴─────────────────────────────────┐
│  @openigtlink/core (top-level)                       │
│  Wire codec — pure, transport-independent            │
│  pack/unpack of bytes. Zero dependencies.            │
└──────────────────────────────────────────────────────┘
```

Each layer depends only on the ones below. Importing only the
top-level package gives you the codec without the network code,
which matters for browser bundles where you may not want or need
WebSocket Server.

## Layer 1: the wire codec — `@openigtlink/core`

The top-level package exports the byte-level codec. Pure, no
I/O, no dependencies.

```typescript
import {
  HEADER_SIZE,                   // 58
  packEnvelope,                  // build a complete framed message
  packHeader,                    // build the 58-byte header alone
  unpackEnvelope,                // parse one framed message
  unpackHeader,                  // parse just the 58-byte header
  unpackMessage,                 // parse + dispatch by type_id
  RawBody,                       // opaque body wrapper for unknown types
  type Envelope,                 // (header, body) pair
  type UnpackOptions,
} from "@openigtlink/core";

// Decode: parse a complete frame, get header + typed body back.
const env = unpackEnvelope(wireBytes);
if (env.header.typeId === "TRANSFORM") {
  // env.body is a Transform instance once messages are registered
}

// Encode: round-trip is byte-exact.
const roundTrip = packEnvelope(env);
// Buffer.compare(wireBytes, roundTrip) === 0
```

The codec validates structure (CRC, framing, body-size, ASCII)
on decode and throws one of the typed errors in
`@openigtlink/core` (`ProtocolError`, `CrcMismatchError`,
`MalformedMessageError`, `ShortBufferError`,
`UnknownMessageTypeError`).

## Layer 2: typed messages — `@openigtlink/core/messages`

84 built-in typed message classes — one per type in the spec —
generated from `spec/schemas/`. Each is a TypeScript class with
spec-aligned field names and types.

```typescript
import { Transform, Status } from "@openigtlink/core/messages";

// Or: import "@openigtlink/core/messages";  // side-effect register all

const tx = new Transform({
  matrix: [1, 0, 0,
           0, 1, 0,
           0, 0, 1,
           10, 20, 30],
});

const bodyBytes = tx.pack();                 // body only, no header
const tx2 = Transform.unpack(bodyBytes);
// tx2.matrix deeply equals tx.matrix

console.log(Transform.TYPE_ID);              // "TRANSFORM"
```

Importing `@openigtlink/core/messages` **as a side effect**
(`import "@openigtlink/core/messages"`) registers all 84
built-ins with the dispatch table — after that, `unpackEnvelope`
returns typed instances rather than `RawBody`. If you only need
a subset, import the specific classes; the side-effect import is
just convenience.

For per-message field details, see
[`../spec/MESSAGES.md`](../spec/MESSAGES.md).

### Registry and dispatch

```typescript
import {
  registerMessageType,
  lookupMessageClass,
  registeredTypes,
} from "@openigtlink/core";

class TrackedFrame {
  static TYPE_ID = "TRACKEDFRAME";
  static unpack(body: Uint8Array): TrackedFrame { /* ... */ }
  pack(): Uint8Array { /* ... */ }
}

registerMessageType(TrackedFrame);
// Now unpackEnvelope returns TrackedFrame instances for that type_id.
```

The contract is:
1. A `static TYPE_ID` constant matching the spec.
2. A `static unpack(body: Uint8Array)` returning the typed instance.
3. An instance `pack(): Uint8Array` for encode.

PLUS's `TRACKEDFRAME`, vendor extensions, research prototypes —
all hook in through this single registration call.

## Layer 3: transports — `@openigtlink/core/net` and `/net/ws`

TCP and WebSocket, client and server. The split between `/net`
and `/net/ws` is environmental: `/net` includes Node-only APIs
(TCP via the `net` module); `/net/ws` is browser-safe WebSocket.
A bundler targeting the browser should import only `/net/ws` to
avoid pulling Node built-ins.

```typescript
// Node (TCP):
import { Client } from "@openigtlink/core/net";
import { Transform, Status } from "@openigtlink/core/messages";

const client = await Client.connect({ host: "tracker.lab", port: 18944 });
await client.send(new Transform({ matrix: [/* ... */] }));
const reply = await client.receive(Status);
console.log(reply.body.statusMessage);

// Browser (WebSocket):
import { WsClient } from "@openigtlink/core/net/ws";
import { Transform, Status } from "@openigtlink/core/messages";

const ws = await WsClient.connect("wss://tracker.lab/igtl");
// ... same send/receive surface
```

Server-side mirrors the client API — `Server.listen({ port })`
for TCP, `WsServer.listen({ port })` for WebSocket. Both expose
`.on(MessageClass, handler)` for type-dispatched receives and
support host/origin allowlists, max-clients caps, and
idle-disconnect.

## Submodules in detail

| Subpath | Purpose |
|---|---|
| `@openigtlink/core` | Top-level codec + registry. Pure, zero deps. |
| `@openigtlink/core/messages` | The 84 typed message classes. Side-effect import to register all. |
| `@openigtlink/core/net` | Node TCP + WebSocket client/server. |
| `@openigtlink/core/net/ws` | Browser-safe WebSocket client (no Node imports). |
| `@openigtlink/core/runtime` | Lower-level primitives — header/extended-header/metadata pack/unpack, CRC, byte order, oracle. Re-exported from the top level; reach in directly only if you need a non-public helper. |

## Type system

The codec uses TypeScript's strict mode and ships full type
declarations. A few conventions worth knowing:

- **64-bit integers** (timestamps, body sizes) decode to `bigint`.
  The codec rejects narrowing to `number` because the wire field
  exceeds `Number.MAX_SAFE_INTEGER` for legitimate timestamps.
- **Variable-count arrays** decode through `DataView` rather than
  view-onto-buffer because the encoded byte order is big-endian
  and the runtime is not (most JS engines are little-endian on
  contemporary x86/ARM). This is a per-element O(N) copy at
  decode time; see [`README.md`](README.md) for the rationale.
- **`Uint8Array` everywhere** for raw bytes, never `Buffer`. The
  package is environment-agnostic; Node code that already has a
  `Buffer` can pass it directly because `Buffer extends Uint8Array`.

## Extension model

Two stable extension points:

- **New message types** via `registerMessageType()`, described
  above. Adds a `type_id` to the registry.
- **New transports** by writing to the codec directly. The
  pack/unpack functions are pure and transport-independent — a
  caller building a UDP backend or a SharedArrayBuffer bridge
  consumes the top-level package without `/net`.

## Where to look for what

| You want to… | Look at |
|---|---|
| Pack/unpack bytes from a buffer you already have | [`README.md`](README.md), top-level package exports |
| Find out what a TRANSFORM looks like on the wire | [`../spec/MESSAGES.md`](../spec/MESSAGES.md) |
| Connect a Node client to an OpenIGTLink server | `@openigtlink/core/net` exports |
| Speak OpenIGTLink from a browser | `@openigtlink/core/net/ws` |
| Add a custom message type | The "Registry and dispatch" section above |
| Tune the bundle for browsers (avoid Node built-ins) | Import only the top-level package and `/net/ws` |
| Understand the cross-language guarantees | [`../spec/CONFORMANCE.md`](../spec/CONFORMANCE.md) |
