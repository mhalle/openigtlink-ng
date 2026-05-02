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
│  oigtl::Client / oigtl::Server   ← what you'll use   │
│  Ergonomic facade — connect, send, receive,         │
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
│  oigtl::runtime::          ← reach in only when      │
│  Pure codec — header, extended header, metadata,     │
│  CRC, byte order, ASCII, dispatch, oracle.           │
└──────────────────────────────────────────────────────┘
```

Each layer depends only on the ones below. The tour goes top-down
— the facade first, because that's where most code lives; the
runtime last, because most callers never touch it directly.

## Connecting and sending: `oigtl::Client` / `oigtl::Server`

The "lovely API" — sync by design (researchers and lab
applications rarely benefit from async-everywhere), templated on
message types. This is what most C++ applications use.

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

## Working with messages: `oigtl::messages`

84 generated message structs — one per type in the spec.
Generated from `spec/schemas/` by the same Python codegen that
produces the Python and TypeScript cores. You include them and
pass instances around; the facade does the encoding/decoding.

```cpp
#include "oigtl/messages/transform.hpp"

using oigtl::messages::Transform;

Transform tx;
tx.matrix = {1, 0, 0, 0, 1, 0, 0, 0, 1, 10, 20, 30};
```

Each message struct exposes:

- A `static constexpr type_id` matching the spec.
- A `pack()` returning `std::vector<std::uint8_t>` (body bytes only).
- A `static unpack(ptr, len)` returning the typed instance, throwing on malformed input.
- Spec-aligned field names and types (`std::array<float, 12>` for TRANSFORM's matrix, etc.).

For per-message field details, see
[`../spec/MESSAGES.md`](../spec/MESSAGES.md).

The registry — `oigtl::messages::default_registry()` — maps
`type_id` strings to round-trip functions. The facade uses it
for `Server::on<T>()` dispatch.

## Reaching deeper

Most application code stops at the facade and the message
structs. The two lower layers are accessible if you have a
genuine need:

### `oigtl::transport` — custom Connection backends

If you need a non-TCP transport (UDP, IPC, a test harness with
programmable failure modes), or you want to drive framing
yourself rather than rely on the facade's receive loop, reach
into `oigtl::transport`. Headers under
[`include/oigtl/transport/`](include/oigtl/transport/):

| Header | What lives here |
|---|---|
| `connection.hpp` | The `Connection` interface — abstract async send/receive. |
| `framer.hpp` | The `Framer` interface — splits a byte stream into IGTL frames. `make_v3_framer(max_body_size)` is the standard implementation. |
| `tcp.hpp` | TCP backend — `make_tcp_pair`, `make_tcp_server`. ASIO under the hood. |
| `future.hpp` | `Future<T>` — small promise/future primitive used to wire async results to callers. |
| `sync.hpp` | Sync helpers — block on a Future with a timeout. |
| `errors.hpp` | Transport-layer exceptions (`FramingError`, `ConnectionClosedError`, …). |
| `policy.hpp` | Server-side connection policy (host allowlists, max-clients, idle-disconnect). |

### `oigtl::runtime` — pure codec

If you need to encode/decode bytes without a transport at all —
file replays, fixture generation, a gateway that parses headers
to route by `type_id` without decoding the body — reach into
`oigtl::runtime`. Pure codec, stdlib-only, bounds-checked,
~700 LoC of hand-written C++17. Headers under
[`include/oigtl/runtime/`](include/oigtl/runtime/):

| Header | What lives here |
|---|---|
| `byte_order.hpp` | Inline big-endian read/write helpers (`read_be_u16`, `write_be_f64`, …). Constexpr where possible. |
| `crc64.hpp` | CRC-64 ECMA-182 over arbitrary bytes. Slice-by-8, ~1.4 GB/s. |
| `header.hpp` | The 58-byte OpenIGTLink header — `pack_header` / `unpack_header`, with type_id and device_name validated as ASCII null-padded. |
| `extended_header.hpp` | The v3 extended-header region (12 bytes). |
| `metadata.hpp` | The v2/v3 metadata block — index + body. Bounds-checked allocation, validated index/body sizes. |
| `ascii.hpp` | ASCII validation primitives. Reject non-ASCII bytes in declared-ASCII fields per the spec. |
| `dispatch.hpp` | `Registry<type_id → round-trip fn>`. |
| `oracle.hpp` | `parse_wire` / `verify_wire_bytes` — the conformance-oracle surface used by the codec, the negative-corpus tests, and the fuzzer. |
| `error.hpp` | Typed exception hierarchy: `ProtocolError`, `ShortBufferError`, `CrcMismatchError`, `MalformedMessageError`, `UnknownMessageTypeError`. |
| `invariants.hpp` | Cross-field invariants (e.g. `image` post-unpack invariant tying `pixels.size()` to header-derived dimensions). |

Direct usage if all you need is bytes-in / bytes-out:

```cpp
#include "oigtl/runtime/oracle.hpp"
#include "oigtl/messages/register_all.hpp"

auto registry = oigtl::messages::default_registry();   // all 84 types
auto result = oigtl::runtime::oracle::verify_wire_bytes(
    wire_ptr, wire_len, registry);
if (!result.ok) { /* result.error */ }
```

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
