# `oigtl` API tour

This is a guided tour of the `oigtl` Python package — what's in it,
how the pieces relate, when to use which. It sits between the quick
usage examples in [`README.md`](README.md) and a full per-symbol
reference (which we don't ship yet — the docstrings on the public
classes are the working substitute).

For message-level questions ("what fields does TRANSFORM have?"),
see [`../spec/MESSAGES.md`](../spec/MESSAGES.md).

For transport details, see [`NET_GUIDE.md`](NET_GUIDE.md).

## Three concentric layers

```
┌─────────────────────────────────────────────────────┐
│  oigtl.net          ← what you'll typically use     │
│  Async + sync clients/servers, TCP + WebSocket,     │
│  resilience, restrictions. See NET_GUIDE.md.        │
└────────────────────┬────────────────────────────────┘
                     │ delegates to
┌────────────────────┴────────────────────────────────┐
│  oigtl.messages                                     │
│  84 built-in typed message classes + an extension   │
│  registration API.                                  │
└────────────────────┬────────────────────────────────┘
                     │ encodes/decodes via
┌────────────────────┴────────────────────────────────┐
│  oigtl  (top-level codec)   ← reach in only if      │
│  Wire codec — pure, transport-independent.          │
│  pack/unpack of bytes. No I/O.                      │
└─────────────────────────────────────────────────────┘
```

Each layer depends only on the ones below. The tour below goes
top-down — `oigtl.net` first, because that's where most code
lives; the codec last, because most callers never touch it
directly.

## Connecting and sending: `oigtl.net`

For researchers, lab integrators, and anyone connecting Python to
an OpenIGTLink peer, `oigtl.net` is the front door. It owns the
sockets, the framing, the receive loop, and the resilience
features.

The minimum viable async client:

```python
from oigtl.net import Client
from oigtl.messages import Transform, Status

async with await Client.connect("tracker.lab", 18944) as c:
    await c.send(Transform(matrix=[1,0,0, 0,1,0, 0,0,1, 0,0,0]))
    reply = await c.receive(Status)
    print(reply.body.status_message)
```

Sync variant for scripts where asyncio would be ceremony:

```python
from oigtl.net import SyncClient

c = SyncClient.connect("tracker.lab", 18944)
c.send(Transform(matrix=[...]))
reply = c.receive(Status, timeout=2)   # 2 s
c.close()
```

The full network surface — dispatch loops, resilience
(auto-reconnect, offline buffer, TCP keepalive), server-side
restriction builders, WebSocket clients and servers, the gateway
pattern — lives in [`NET_GUIDE.md`](NET_GUIDE.md). Public
entry points:

```python
from oigtl.net import (
    Client, ClientOptions,         # async TCP client
    Server,                        # async TCP server
    SyncClient, SyncServer,        # blocking variants
    WsClient, WsServer,            # WebSocket variants
    Envelope,                      # parsed (header, body) pair
)
```

Receive APIs accept either `timeout=<seconds>` or
`timeout_ms=<ms>` (mutually exclusive). `ClientOptions` does the
same for every duration field — see
[`NET_GUIDE.md`](NET_GUIDE.md) for the rationale.

## Working with messages: `oigtl.messages`

84 built-in typed message classes — one per type in the spec —
generated from `spec/schemas/`. Each is a Pydantic model with
spec-aligned field names and types. You import them and pass
them around; `oigtl.net` does the encoding/decoding.

```python
from oigtl.messages import Transform, Status, Image
# (or any of the 81 others)

# Pydantic validates field types up front.
tx = Transform(matrix=[1.0, 0.0, 0.0,
                        0.0, 1.0, 0.0,
                        0.0, 0.0, 1.0,
                        10.0, 20.0, 30.0])

# Class-level type identifier — what goes on the wire.
assert Transform.TYPE_ID == "TRANSFORM"
```

Every message class also exposes `pack(self) -> bytes` and a
`classmethod unpack(body)` — useful for testing or for the
"reaching deeper" cases below, but you don't need them for
ordinary client/server work.

The schemas under `spec/schemas/` and the rendered reference in
[`../spec/MESSAGES.md`](../spec/MESSAGES.md) document field
semantics, sizes, and version applicability.

### Adding a custom message type

The package ships a registry that maps `type_id` → message class.
Custom types — PLUS's `TRACKEDFRAME`, vendor extensions, research
prototypes — register through it:

```python
from oigtl import register_message_type

class TrackedFrame(BaseModel):
    TYPE_ID = "TRACKEDFRAME"
    @classmethod
    def unpack(cls, body: bytes) -> "TrackedFrame": ...
    def pack(self) -> bytes: ...

register_message_type(TrackedFrame.TYPE_ID, TrackedFrame)
# After this, Client.receive(TrackedFrame) works exactly like the
# built-ins. No transport changes, no library fork.
```

The contract a registered class satisfies: a `TYPE_ID` constant,
a `pack()` instance method returning bytes, a `unpack(body)`
classmethod returning the typed instance.

## Reaching deeper: the wire codec

The `oigtl` top-level package also exports the byte-level codec
directly. Most callers never touch this — `oigtl.net` does it
for you. Reach in only if you're building something `oigtl.net`
doesn't cover:

- A bridge that forwards OpenIGTLink frames over a non-TCP
  transport (MQTT, ROS topics, IPC).
- A file-replay tool that decodes wire bytes captured to disk.
- A browser bundle via Pyodide that needs the codec without the
  Python `socket` module.
- A test harness that produces wire bytes directly to exercise a
  peer's parser.

```python
from oigtl import (
    HEADER_SIZE,           # 58 — the fixed-header size in bytes
    Header,                # parsed-header dataclass
    RawBody,               # opaque body wrapper for unknown type_ids
    pack_envelope,         # build a complete framed message
    pack_header,           # build the 58-byte header alone
    unpack_envelope,       # parse one framed message
    unpack_header,         # parse just the 58-byte header
    unpack_message,        # parse + dispatch by type_id
)

# Build a framed message from a typed body.
wire = pack_envelope(transform_body, device_name="tracker_1")

# Parse a complete frame.
header, body = unpack_envelope(wire)
```

The codec validates structure (CRC, framing, body-size, ASCII)
on decode and raises one of the typed exceptions in
`oigtl.runtime.exceptions` (`ProtocolError`, `CrcMismatchError`,
`MalformedMessageError`, `ShortBufferError`,
`UnknownMessageTypeError`).

## Submodules in detail

Beyond the three top-level layers, the package exposes a few
specialised submodules. Most callers don't need them directly,
but they're the right place to look for narrow needs.

| Module | Purpose |
|---|---|
| `oigtl.codec` | The pack/unpack functions re-exported by the top-level `oigtl` package. |
| `oigtl.messages` | The 84 typed classes and the registry (also exported at top level). |
| `oigtl.runtime` | Lower-level building blocks: header parsing, byte-order helpers, exception types, IGTL array helpers. |
| `oigtl.runtime.envelope` | The `Envelope` and `RawMessage` types used by the transport layer. |
| `oigtl.runtime.exceptions` | The typed exception hierarchy. Catch `ProtocolError` for any spec violation. |
| `oigtl.semantic` | Parsing of human-meaningful semantic JSON for round-trip testing. Internal-ish but stable. |
| `oigtl.net.errors` | Network-layer exceptions (`ConnectionClosedError`, `BufferOverflowError`, `TimeoutError`). |
| `oigtl.net.gateway` | Protocol-bridging utilities for two-way relay and translation. |
| `oigtl.net.interfaces` | Local-host network introspection (`primary_address()`, `subnets()`). |
| `oigtl.net.policy` | Server-side connection policy (allowlists, restrictions). Builder API on `Server`. |

## Extension model

Two stable extension points:

- **New message types** via `register_message_type()`, described
  above. Adds a `type_id` to the registry.
- **New transports** by writing to the codec directly. The
  pack/unpack functions are pure and transport-independent, so a
  caller building (e.g.) a UDP backend or an MQTT bridge can
  consume `oigtl.codec` without touching `oigtl.net` at all.

The codec itself is *not* designed to be extended (the wire
format is the wire format), but the layers above it are.

## Versioning

The transport layer made one breaking change in v0.4.0: bare
numbers on duration fields are now seconds rather than
milliseconds, and `*_ms` companion fields exist for the
millisecond idiom. See [`NET_GUIDE.md`](NET_GUIDE.md) §"Duration
parameters" and the `CHANGELOG.md` entry.

The codec layer has been API-stable since v0.1; the only
prospective breaking changes are `__all__` additions and
extension-API enrichment, both of which are additive.

## Where to look for what

| You want to… | Look at |
|---|---|
| Connect a Python client to an OpenIGTLink server | [`NET_GUIDE.md`](NET_GUIDE.md) |
| Run a resilient client across flaky networks | [`NET_GUIDE.md`](NET_GUIDE.md) §"Resilient client" |
| Restrict who can connect to your server | [`NET_GUIDE.md`](NET_GUIDE.md) §"Server with host-level restrictions" |
| Speak OpenIGTLink from a browser | [`NET_GUIDE.md`](NET_GUIDE.md) §"WebSocket" + `WsClient` |
| Find out what a TRANSFORM looks like on the wire | [`../spec/MESSAGES.md`](../spec/MESSAGES.md) |
| Add a custom message type | The "Adding a custom message type" section above |
| Pack/unpack bytes without a transport | "Reaching deeper" above |
| Understand the cross-language guarantees | [`../spec/CONFORMANCE.md`](../spec/CONFORMANCE.md) |
