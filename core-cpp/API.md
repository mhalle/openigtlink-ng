# `oigtl::` C++ API tour

This is a guided tour of the modern `oigtl::` C++ API — what's in
it, how the pieces relate, when to use which. It sits between the
quick examples in [`README.md`](README.md) and per-symbol
documentation in the headers themselves.

If you are porting an application written against upstream's
`igtl::` C++ API, you do **not** need this document — see
[`compat/`](compat/) and [`compat/MIGRATION.md`](compat/MIGRATION.md).
The compat shim re-exposes upstream's API on top of the codec
described here. This tour is for new code.

For message-level questions ("what fields does TRANSFORM have?"),
see [`../spec/MESSAGES.md`](../spec/MESSAGES.md).

For the resilient-client surface (auto-reconnect, offline buffer,
TCP keepalive), see [`CLIENT_GUIDE.md`](CLIENT_GUIDE.md).

## Layered structure

```
┌──────────────────────────────────────────────────────┐
│  oigtl::Client / oigtl::Server                       │
│  Ergonomic facade — connect, send, receive,          │
│  on<T>() dispatch, restrict_to_local_subnet, etc.    │
└────────────────────┬─────────────────────────────────┘
                     │ wraps
┌────────────────────┴─────────────────────────────────┐
│  oigtl::transport::                                  │
│  Connection, Framer, Future<T>, sync helpers,        │
│  ASIO-backed TCP, in-process loopback.               │
└────────────────────┬─────────────────────────────────┘
                     │ uses
┌────────────────────┴─────────────────────────────────┐
│  oigtl::messages::                                   │
│  84 generated message structs + a Registry that      │
│  maps type_id → round-trip function.                 │
└────────────────────┬─────────────────────────────────┘
                     │ encodes/decodes via
┌────────────────────┴─────────────────────────────────┐
│  oigtl::runtime::                                    │
│  Pure codec — header, extended header, metadata,     │
│  CRC, byte order, ASCII, dispatch, oracle.           │
│  Bounds-checked, fuzzer-hardened, stdlib-only.       │
└──────────────────────────────────────────────────────┘
```

Each layer depends only on the ones below. You can stop at any
of them — the runtime alone is enough if you have your own
transport; the runtime + messages is enough if you only need to
encode/decode bytes; the full stack is the path of least
resistance for new applications.

## Layer 1: the runtime — `oigtl::runtime`

Pure codec, stdlib-only, bounds-checked, ~700 LoC of
hand-written C++17. Headers under
[`include/oigtl/runtime/`](include/oigtl/runtime/):

| Header | What lives here |
|---|---|
| `byte_order.hpp` | Inline big-endian read/write helpers (`read_be_u16`, `write_be_f64`, …). Constexpr where possible. |
| `crc64.hpp` | CRC-64 ECMA-182 over arbitrary bytes. Slice-by-8 implementation, ~1.4 GB/s on contemporary hardware. |
| `header.hpp` | The 58-byte OpenIGTLink header — `pack_header` / `unpack_header`, with type_id and device_name validated as ASCII null-padded. |
| `extended_header.hpp` | The v3 extended-header region (12 bytes). Used by `messages::` for v2/v3 framing. |
| `metadata.hpp` | The v2/v3 metadata block — index + body. Bounds-checked allocation, validated index/body sizes. |
| `ascii.hpp` | ASCII validation primitives. Reject non-ASCII bytes in declared-ASCII fields per the spec. |
| `dispatch.hpp` | `Registry<type_id → round-trip fn>`. The dispatch table for `unpack_message` and the oracle. |
| `oracle.hpp` | `parse_wire` / `verify_wire_bytes` — the conformance-oracle surface used by the codec, the negative-corpus tests, and the fuzzer. |
| `error.hpp` | Typed exception hierarchy: `ProtocolError`, `ShortBufferError`, `CrcMismatchError`, `MalformedMessageError`, `UnknownMessageTypeError`. |
| `invariants.hpp` | Cross-field invariants (e.g. `image` post-unpack invariant tying `pixels.size()` to header-derived dimensions). One named invariant per spec rule, implemented by every codec. |

Direct usage if all you need is bytes-in / bytes-out:

```cpp
#include "oigtl/runtime/header.hpp"
#include "oigtl/runtime/oracle.hpp"
#include "oigtl/messages/register_all.hpp"

auto registry = oigtl::messages::default_registry();   // all 84 types
auto result = oigtl::runtime::oracle::verify_wire_bytes(
    wire_ptr, wire_len, registry);
if (!result.ok) { /* result.error */ }
```

## Layer 2: typed messages — `oigtl::messages`

84 generated message structs — one per type in the spec.
Generated from `spec/schemas/` by the same Python codegen that
produces the Python and TypeScript cores.

```cpp
#include "oigtl/messages/transform.hpp"

using oigtl::messages::Transform;

Transform tx;
tx.matrix = {1, 0, 0, 0, 1, 0, 0, 0, 1, 10, 20, 30};

auto body_bytes = tx.pack();           // bytes only, no header
auto tx2 = Transform::unpack(body_bytes.data(), body_bytes.size());
```

Each message struct has:

- A `static constexpr type_id` matching the spec.
- A `pack()` returning `std::vector<std::uint8_t>`.
- A `static unpack(ptr, len)` returning the typed instance, throwing on malformed input.
- Spec-aligned field names and types (`std::array<float, 12>` for TRANSFORM's matrix, etc.).

The registry — `oigtl::messages::default_registry()` — maps
`type_id` strings to round-trip functions. Used by `unpack_message`,
the oracle, the fuzzer, and the `Server::on<T>()` dispatcher
described below.

For per-message field details, see
[`../spec/MESSAGES.md`](../spec/MESSAGES.md).

## Layer 3: transport — `oigtl::transport`

Asynchronous Connection abstraction, message framing, and a
small Future primitive. ASIO-backed TCP plus an in-process
loopback for tests. Headers under
[`include/oigtl/transport/`](include/oigtl/transport/):

| Header | What lives here |
|---|---|
| `connection.hpp` | The `Connection` interface — abstract async send/receive. TCP and loopback both implement it. |
| `framer.hpp` | The `Framer` interface — splits a byte stream into IGTL frames. `make_v3_framer(max_body_size)` produces the standard implementation. |
| `tcp.hpp` | TCP backend — `make_tcp_pair`, `make_tcp_server`. ASIO under the hood. |
| `future.hpp` | `Future<T>` — a tiny promise/future primitive used to wire async results back to the caller. |
| `sync.hpp` | Sync helpers — block on a Future with a timeout. |
| `errors.hpp` | Transport-layer exceptions (`FramingError`, `ConnectionClosedError`, …). |
| `policy.hpp` | Server-side connection policy (host allowlists, max-clients, idle-disconnect). Builder API exposed via `Server`. |

Most application code does not call into this layer directly —
it sits behind `oigtl::Client` / `oigtl::Server` (next layer).
Reach into `oigtl::transport` if you need a custom Connection
backend or want to drive framing yourself.

## Layer 4: the ergonomic facade — `oigtl::Client` / `oigtl::Server`

The "lovely API" that sits on top of transport + codec. Sync, by
design — researchers and lab applications rarely benefit from
async-everywhere. Templated on message types.

```cpp
#include "oigtl/client.hpp"
#include "oigtl/server.hpp"
#include "oigtl/messages/transform.hpp"
#include "oigtl/messages/status.hpp"

using oigtl::messages::Transform;
using oigtl::messages::Status;

// Client: connect, send, receive.
auto client = oigtl::Client::connect("tracker.lab", 18944);
client.send(Transform{ /* matrix = */ { ... } });
auto reply = client.receive<Status>();

// Server: listen, dispatch by type, run.
oigtl::Server::listen(18944)
    .on<Transform>([&](auto& env) {
        process_pose(env.body.matrix);
    })
    .on<Status>([&](auto& env) {
        log_status(env.body.status_message);
    })
    .restrict_to_local_subnet()        // opt-in network policy
    .set_max_simultaneous_clients(4)
    .run();
```

`ClientOptions` (declared in `client.hpp`) controls timeouts,
auto-reconnect, offline buffer, TCP keepalive — see
[`CLIENT_GUIDE.md`](CLIENT_GUIDE.md) for the resilient-client
surface in detail.

`Envelope<T>` is a typed `(Header, T body)` pair — what every
`receive()` and dispatch handler gets.

## Putting it together

For a new C++ application:

1. **Use the facade.** `oigtl::Client::connect(...)`,
   `oigtl::Server::listen(...).on<T>(...)`. That's enough for
   most workflows.
2. **Reach into `oigtl::messages` if you need to encode/decode
   without a transport** — file replays, custom protocols
   layered on top, fixture generation.
3. **Reach into `oigtl::runtime` if you need to validate bytes
   without instantiating typed messages** — for example, a
   gateway that parses a header to route by `type_id` without
   decoding the body.
4. **Reach into `oigtl::transport` if you need a custom
   Connection backend** — UDP, IPC, a test harness with
   programmable failure modes.

## Headers cheatsheet

If you remember nothing else from this tour:

```cpp
#include "oigtl/client.hpp"            // oigtl::Client + ClientOptions
#include "oigtl/server.hpp"            // oigtl::Server + builders
#include "oigtl/envelope.hpp"          // oigtl::Envelope<T>
#include "oigtl/messages/transform.hpp"  // (or any other message type)
#include "oigtl/messages/register_all.hpp"  // default_registry()
#include "oigtl/runtime/oracle.hpp"    // verify_wire_bytes
#include "oigtl/runtime/error.hpp"     // typed exceptions
```

## Build + link

`CMakeLists.txt` ships an `oigtl::oigtl` interface target. The
typical consumer:

```cmake
find_package(oigtl REQUIRED)
target_link_libraries(my_app PRIVATE oigtl::oigtl)
```

The single static archive `liboigtl.a` carries the whole
project — runtime, messages, transport, and the compat shim.
For drop-in replacement of upstream's `libOpenIGTLink.a` (so PLUS
or another consumer's existing CMake finds us as upstream), see
[`compat/MIGRATION.md`](compat/MIGRATION.md) §"Build recipes".

## Where to look for what

| You want to… | Look at |
|---|---|
| Connect a C++ client to an OpenIGTLink server | [`README.md`](README.md), `oigtl/client.hpp` |
| Run a resilient client across flaky networks | [`CLIENT_GUIDE.md`](CLIENT_GUIDE.md) |
| Restrict who can connect to your server | `oigtl/server.hpp` builder methods |
| Find out what a TRANSFORM looks like on the wire | [`../spec/MESSAGES.md`](../spec/MESSAGES.md) |
| Pack/unpack bytes without a transport | `oigtl::messages::T::pack()` / `T::unpack()` |
| Validate framing/CRC without decoding the body | `oigtl::runtime::oracle::verify_wire_bytes` |
| Port code written against upstream's `igtl::` API | [`compat/MIGRATION.md`](compat/MIGRATION.md) |
| Port PLUS Toolkit specifically | [`compat/PORTING_PLUS.md`](compat/PORTING_PLUS.md) |
| Understand the cross-language guarantees | [`../spec/CONFORMANCE.md`](../spec/CONFORMANCE.md) |
