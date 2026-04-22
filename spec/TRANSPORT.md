# Transport

The wire format specified in [`protocol/v3.md`](protocol/v3.md)
is transport-agnostic — an OpenIGTLink frame is a 58-byte header
plus a body, and those bytes can travel over any stream that
preserves ordering and delivers them intact. In practice, two
transports matter for this project:

- **TCP** — the canonical OpenIGTLink transport; what upstream
  uses and what all existing deployments speak.
- **WebSocket** — lets browsers (and other environments that
  can't open raw TCP) participate. The framing carries a
  complete OpenIGTLink frame per WebSocket binary message, so
  the wire bytes are identical to TCP; only the outer layer
  differs.

This document describes the shared shape of the transport
layer across the three typed cores, so a contributor thinking
"how should this work?" has a single reference instead of three
per-core plan documents.

For language-specific details, see:

- [`core-py/NET_GUIDE.md`](/core-py/NET_GUIDE.md)
- [`core-ts/README.md`](/core-ts/README.md) (transport section)
- [`core-cpp/CLIENT_GUIDE.md`](/core-cpp/CLIENT_GUIDE.md)

---

## What every core provides

Every typed core exposes the same conceptual surface, spelled
idiomatically for its language:

- **Client** — connects to a remote peer, sends and receives
  typed envelopes. Both async/non-blocking and a synchronous
  wrapper where idiomatic.
- **Server** — binds a port, accepts peers, dispatches received
  messages to registered handlers (one per message type).
- **Framing** — a streaming reassembler that pulls complete
  OpenIGTLink frames out of a byte stream (TCP or WebSocket
  binary-message chunks). In all three typed cores this lives
  in a `framer` / `V3Framer` / `ByteAccumulator` primitive
  that's decoupled from the socket so tests can drive it
  directly.
- **Envelope** — a typed message (a generated `*Message` class)
  paired with its header fields (device name, timestamp,
  metadata map). Send and receive operate on envelopes, not
  raw bytes.

The three cores were built to be interop-compatible; the
pairwise test matrix (see below) enforces this.

---

## Transport matrix

Which transports each core supports as **client** and as
**server**:

| Core | TCP client | TCP server | WS client | WS server |
| --- | :---: | :---: | :---: | :---: |
| [`core-py/`](/core-py/) | ✅ | ✅ | ✅ | — |
| [`core-ts/`](/core-ts/) | ✅ | ✅ | ✅ | ✅ |
| [`core-cpp/`](/core-cpp/) | ✅ | ✅ | — | — |

The two gaps reflect deliberate choices:

- **No WebSocket server in core-py.** Python deployments
  typically don't need to serve browsers directly; core-ts
  covers that case. The py WebSocket client covers the
  "Python program talks to a browser-served game of
  OpenIGTLink" direction.
- **No WebSocket client/server in core-cpp.** C++ consumers
  are overwhelmingly bare-TCP applications (Slicer, PLUS,
  device firmware). Adding WebSocket to the C++ core would
  pull in a WS library dependency we don't otherwise need.

These aren't permanent — either could be added without
reshaping the architecture — but nothing in the current
roadmap requires them.

---

## Cross-language interop

Every pair of cores that both implement a given transport is
tested end-to-end. The test suite spins up a server in one
language, connects a client in another, round-trips a message,
and asserts byte-identical behavior.

Pairings currently exercised in CI:

| Client → Server | TCP | WebSocket |
| --- | :---: | :---: |
| py → ts | ✅ | ✅ |
| py → cpp | ✅ | — |
| ts → py | ✅ | — |
| ts → cpp | ✅ | — |
| cpp → py | ✅ | — |
| cpp → ts | ✅ | — |

The TCP matrix is complete. The WebSocket row exercises the
one pairing that matters operationally (Python client talking
to a TypeScript-served browser environment).

Test files live under `core-*/tests/cross_runtime_*` and drive
real subprocesses speaking real sockets — not in-process
mocks. A language's fixture (e.g.,
`core-ts/tests/net/fixtures/ts_tcp_echo.ts`) is a standalone
process that prints `PORT=<n>` to stdout, accepts one
connection, and echoes TRANSFORM messages as STATUS. The
peer-language test spawns it, reads the port, and drives the
interop. This pattern is repeated across every core.

---

## Resilience features

The transport layer — not the codec — is where real-world
deployments diverge from the happy path. The typed cores ship
the same resilience primitives, with the same shape, so a
contributor who learned them in one language can predict how
they behave in another.

- **Auto-reconnect.** `Client` supports transparent
  reconnection with exponential backoff. If the peer goes
  away, the client's `send` blocks (or queues, see below)
  rather than erroring out. Callers who *want* connection
  errors surfaced can disable this via options.
- **TCP keepalive.** Kernel-level keepalive is enabled by
  default with tuneable intervals. Catches silently-dropped
  NAT sessions before the application layer has to.
- **Offline outgoing buffer.** A bounded in-memory queue
  holds messages that couldn't be sent (peer disconnected).
  On reconnect, the buffer drains in order. Options control
  the cap, the policy on overflow (drop-oldest vs. block), and
  whether to buffer at all.
- **Rate limiting and per-peer resource caps (server-side).**
  The servers enforce configurable bounds on connection count,
  per-connection message rate, and per-peer memory footprint.
  Defaults are conservative; production deployments should
  review them.

These are documented in each core's guide; the APIs differ in
idiom (asyncio options, C++ `ClientOptions`, TypeScript option
objects) but the semantics match. Cross-language differences
should be bugs.

---

## What transport does *not* provide (yet)

This layer is **over trusted network segments** until further
notice. Specifically missing:

- **TLS.** No encrypted transport. OpenIGTLink today is
  unencrypted by protocol; adding TLS is designed (see
  [`core-cpp/docs/history/transport_plan.md`](/core-cpp/docs/history/transport_plan.md))
  but not implemented.
- **Authentication.** No peer identity verification.
  Connections are accepted on network reachability alone.
- **Session state above framing.** Each connection carries a
  stream of independent messages; there's no session-level
  sequencing, message ACK, or resumption.

Deployments should continue to rely on network segmentation
(VLANs, dedicated subnets, firewall rules) until these land.
This is the same posture as upstream OpenIGTLink; we don't make
it worse but we also haven't yet made it better.

---

## Framing details (shared)

All three cores implement the same streaming reassembler:

1. Read bytes into an accumulator.
2. When at least 58 bytes are buffered, peek the fixed header
   and determine `body_size`.
3. Wait until `58 + body_size` bytes are present, then emit a
   complete frame.
4. Reset and continue.

The accumulator *never* consumes bytes speculatively — if a
partial frame is buffered, bytes stay in the accumulator until
a full frame can be delivered. This is a deliberate safety
choice: it lets the parser's bounds checks fire on complete
frames only, rather than on partial input that happens to
pass early validation.

Framing code lives at:
- [`core-cpp/include/oigtl/runtime/framer.hpp`](/core-cpp/include/oigtl/runtime/framer.hpp)
- [`core-py/src/oigtl/net/framer.py`](/core-py/src/oigtl/net/framer.py)
- [`core-ts/src/net/framer.ts`](/core-ts/src/net/framer.ts)

Each is fuzz-tested as part of the differential harness; the
three implementations are held byte-identical on the same
input stream.

---

## If you're adding a new transport

Hypothetically — say, QUIC or a custom RF link. The separation
that makes this additive:

- The framer is pure: bytes in, frames out, no I/O.
- The codec operates on byte buffers, not on streams.
- `Client` / `Server` are thin wrappers around a transport-
  specific read/write loop plus the framer plus the codec.

You'd write a new transport module that calls the same framer
and codec, and expose it with the same `Client` / `Server`
surface. The existing tests in each language (round-trip,
envelope, framer unit tests) provide ~90% of the coverage for
free; you'd add new interop tests for your transport pairing
with the existing ones.

The per-core design records under
[`core-cpp/docs/history/`](/core-cpp/docs/history/),
[`core-py/docs/history/`](/core-py/docs/history/), and
[`core-ts/docs/history/`](/core-ts/docs/history/) capture the
design reasoning for each language core's specific choices when
its transport was first built. They're retained as decision
records.
