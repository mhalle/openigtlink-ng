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
│  oigtl.net                                          │
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
│  oigtl  (top-level codec)                           │
│  Wire codec — pure, transport-independent           │
│  pack/unpack of bytes. No I/O.                      │
└─────────────────────────────────────────────────────┘
```

Each layer depends only on the ones below. You can use the codec
without the messages layer; you can use the messages layer without
the net layer; you can wire your own transport on top of the
codec if you want.

## Layer 1: the wire codec — `oigtl`

The top-level `oigtl` package exports the byte-level codec.
Sufficient on its own for MQTT payloads, file replays, browser
bundles via Pyodide, or any caller that already holds raw bytes.

```python
from oigtl import (
    HEADER_SIZE,           # 58 — the fixed-header size in bytes
    Header,                # parsed-header dataclass
    RawBody,               # opaque body wrapper
    pack_envelope,         # build a complete framed message
    pack_header,           # build the 58-byte header alone
    unpack_envelope,       # parse one framed message
    unpack_header,         # parse just the 58-byte header
    unpack_message,        # parse + dispatch by type_id
)
```

Two idioms cover most callers:

```python
# Encode: build a framed message directly from a typed body.
wire = pack_envelope(transform_body, device_name="tracker_1")

# Decode: parse a complete frame, get header + typed body back.
header, body = unpack_envelope(wire)
```

The codec validates structure (CRC, framing, body-size, ASCII)
on decode and raises one of the typed exceptions in
`oigtl.runtime.exceptions` (`ProtocolError`, `CrcMismatchError`,
`MalformedMessageError`, `ShortBufferError`, `UnknownMessageTypeError`).

## Layer 2: typed messages — `oigtl.messages`

84 built-in typed message classes — one per type in the spec —
generated from `spec/schemas/`. Each is a Pydantic model with
spec-aligned field names and types.

```python
from oigtl.messages import Transform, Status, Image
# (or any of the 81 others)

# Construction — Pydantic validates field types up front.
tx = Transform(matrix=[1.0, 0.0, 0.0,
                        0.0, 1.0, 0.0,
                        0.0, 0.0, 1.0,
                        10.0, 20.0, 30.0])

# Direct pack/unpack — body bytes only, no header.
body_bytes = tx.pack()
tx2 = Transform.unpack(body_bytes)
assert tx == tx2

# Class-level type identifier — what goes on the wire.
assert Transform.TYPE_ID == "TRANSFORM"
```

Every built-in message has the same surface: `TYPE_ID`, `pack()`,
`unpack(body)`, plus the per-message Pydantic fields. The schemas
under `spec/schemas/` and the rendered reference in
[`../spec/MESSAGES.md`](../spec/MESSAGES.md) document field
semantics, sizes, and version applicability.

### Registry and dispatch

The package ships a registry that maps `type_id` → message class.
`unpack_envelope` consults it to choose the right class for an
incoming frame. The registry is also the extension point:

```python
from oigtl import register_message_type, registered_types

class TrackedFrame(BaseModel):
    TYPE_ID = "TRACKEDFRAME"
    @classmethod
    def unpack(cls, body: bytes) -> "TrackedFrame": ...
    def pack(self) -> bytes: ...

register_message_type(TrackedFrame.TYPE_ID, TrackedFrame)
# Now unpack_envelope returns TrackedFrame instances for that type_id,
# and Client.receive(TrackedFrame) works the same as for built-ins.
```

PLUS's `TRACKEDFRAME`, vendor extensions, research prototypes —
all hook in through this single registration call. No
transport changes, no special-casing, no library fork.

## Layer 3: transports — `oigtl.net`

Async + sync TCP and WebSocket clients/servers, with optional
resilience features (auto-reconnect, offline buffer, TCP
keepalive) and a server-side restriction builder for host/subnet
allowlists, max-clients caps, and idle-disconnect.

The full surface is its own document — see
[`NET_GUIDE.md`](NET_GUIDE.md) — but the entry points are:

```python
from oigtl.net import (
    Client, ClientOptions,         # async TCP client
    Server,                        # async TCP server
    SyncClient, SyncServer,        # blocking variants of the above
    WsClient, WsServer,            # WebSocket variants (browsers, dashboards)
    Envelope,                      # parsed (header, body) pair
)
```

Minimum-viable async client (matching NET_GUIDE.md's opening):

```python
from oigtl.net import Client
from oigtl.messages import Transform, Status

async with await Client.connect("tracker.lab", 18944) as c:
    await c.send(Transform(matrix=[1,0,0, 0,1,0, 0,0,1, 0,0,0]))
    reply = await c.receive(Status)
    print(reply.body.status_message)
```

Receive APIs accept either `timeout=<seconds>` or
`timeout_ms=<ms>` (mutually exclusive). `ClientOptions` does the
same for every duration field — see
[`NET_GUIDE.md`](NET_GUIDE.md) for the rationale.

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
| Pack/unpack bytes from a buffer you already have | [`README.md`](README.md), the `oigtl.codec` exports |
| Find out what a TRANSFORM looks like on the wire | [`../spec/MESSAGES.md`](../spec/MESSAGES.md) |
| Connect a Python client to an OpenIGTLink server | [`NET_GUIDE.md`](NET_GUIDE.md) |
| Add a custom message type | The "Registry and dispatch" section above |
| Run a resilient client across flaky networks | [`NET_GUIDE.md`](NET_GUIDE.md) §"Resilient client" |
| Restrict who can connect to your server | [`NET_GUIDE.md`](NET_GUIDE.md) §"Server with host-level restrictions" |
| Speak OpenIGTLink from a browser | [`NET_GUIDE.md`](NET_GUIDE.md) §"WebSocket" + `WsClient` |
| Understand the cross-language guarantees | [`../spec/CONFORMANCE.md`](../spec/CONFORMANCE.md) |
