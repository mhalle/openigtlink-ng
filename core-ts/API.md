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
│  @openigtlink/core/net           ← what you'll use   │
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
│  @openigtlink/core (top-level)   ← reach in only     │
│  Wire codec — pure, transport-independent.           │
│  pack/unpack of bytes. Zero dependencies.            │
└──────────────────────────────────────────────────────┘
```

Each layer depends only on the ones below. The tour goes top-down
— `/net` first, because that's where most code lives; the codec
last, because most callers never touch it directly.

## Connecting and sending: `@openigtlink/core/net` and `/net/ws`

For most callers — researchers, lab integrators, browser
dashboards — the network layer is the front door. Two
environment-specific entry points:

```typescript
// Node (TCP):
import { Client } from "@openigtlink/core/net";
import { Transform, Status } from "@openigtlink/core/messages";

const client = await Client.connect({ host: "tracker.lab", port: 18944 });
await client.send(new Transform({ matrix: [1, 0, 0,
                                            0, 1, 0,
                                            0, 0, 1,
                                            10, 20, 30] }));
const reply = await client.receive(Status);
console.log(reply.body.statusMessage);

// Browser (WebSocket):
import { WsClient } from "@openigtlink/core/net/ws";
import { Transform, Status } from "@openigtlink/core/messages";

const ws = await WsClient.connect("wss://tracker.lab/igtl");
// ... same send/receive surface
```

The split between `/net` and `/net/ws` is environmental: `/net`
includes Node-only APIs (TCP via the `net` module); `/net/ws` is
browser-safe WebSocket. A bundler targeting the browser should
import only `/net/ws` to avoid pulling Node built-ins.

Server-side mirrors the client API — `Server.listen({ port })`
for TCP, `WsServer.listen({ port })` for WebSocket. Both expose
`.on(MessageClass, handler)` for type-dispatched receives and
support host/origin allowlists, max-clients caps, and
idle-disconnect.

## Working with messages: `@openigtlink/core/messages`

84 built-in typed message classes — one per type in the spec —
generated from `spec/schemas/`. Each is a TypeScript class with
spec-aligned field names and types. You import them and pass
instances to the network layer.

```typescript
import { Transform, Status } from "@openigtlink/core/messages";

const tx = new Transform({
  matrix: [1, 0, 0,
           0, 1, 0,
           0, 0, 1,
           10, 20, 30],
});

console.log(Transform.TYPE_ID);              // "TRANSFORM"
```

Importing `@openigtlink/core/messages` **as a side effect**
(`import "@openigtlink/core/messages"`) registers all 84
built-ins with the dispatch table — after that, the network
layer returns typed instances on receive. If you only need a
subset, import the specific classes; the side-effect import is
just convenience.

Every message class also exposes `pack()` and a `static
unpack(body)` — useful for testing or for the "reaching deeper"
cases below, but not needed for ordinary client/server work.

For per-message field details, see
[`../spec/MESSAGES.md`](../spec/MESSAGES.md).

### Adding a custom message type

```typescript
import { registerMessageType } from "@openigtlink/core";

class TrackedFrame {
  static TYPE_ID = "TRACKEDFRAME";
  static unpack(body: Uint8Array): TrackedFrame { /* ... */ }
  pack(): Uint8Array { /* ... */ }
}

registerMessageType(TrackedFrame);
// Receive loops now return TrackedFrame instances for that type_id.
```

The contract a registered class satisfies:
1. A `static TYPE_ID` constant matching the spec.
2. A `static unpack(body: Uint8Array)` returning the typed instance.
3. An instance `pack(): Uint8Array` for encode.

PLUS's `TRACKEDFRAME`, vendor extensions, research prototypes —
all hook in through this single registration call.

## Reaching deeper: the wire codec

The top-level `@openigtlink/core` package also exports the
byte-level codec directly. Most callers never touch this — the
network layer does it for you. Reach in only if you're building
something the network layer doesn't cover:

- A bridge that forwards OpenIGTLink frames over a non-WS
  transport (WebRTC data channel, MQTT-over-WebSocket, IPC).
- A browser bundle that needs the codec without any transport.
- A test harness producing wire bytes to exercise a peer's parser.
- A file-replay tool decoding wire captures from disk.

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

// Decode: parse a complete frame.
const env = unpackEnvelope(wireBytes);

// Encode: round-trip is byte-exact.
const roundTrip = packEnvelope(env);
// Buffer.compare(wireBytes, roundTrip) === 0
```

The codec validates structure (CRC, framing, body-size, ASCII)
on decode and throws one of the typed errors
(`ProtocolError`, `CrcMismatchError`, `MalformedMessageError`,
`ShortBufferError`, `UnknownMessageTypeError`).

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
| Connect a Node client to an OpenIGTLink server | `@openigtlink/core/net` exports |
| Speak OpenIGTLink from a browser | `@openigtlink/core/net/ws` |
| Find out what a TRANSFORM looks like on the wire | [`../spec/MESSAGES.md`](../spec/MESSAGES.md) |
| Add a custom message type | The "Adding a custom message type" section above |
| Pack/unpack bytes without a transport | "Reaching deeper" above |
| Tune the bundle for browsers (avoid Node built-ins) | Import only the top-level package and `/net/ws` |
| Understand the cross-language guarantees | [`../spec/CONFORMANCE.md`](../spec/CONFORMANCE.md) |
