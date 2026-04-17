# C++ transport + upstream-compatible shim — plan

Status: **draft, pre-implementation.**

Two goals, co-designed so one doesn't constrain the other:

1. **Native transport.** A clean, async, future-based C++17 library
   for sending and receiving OpenIGTLink messages over TCP + TLS.
   Holds the five contract properties from the project discussion:
   stateful connection, opaque messages, pluggable framing, async/
   future-based API, TLS as a wrapper.
2. **Upstream-compatible shim.** A second static library that exposes
   the exact `igtl::` API surface of the pinned upstream reference
   library (`corpus-tools/reference-libs/openigtlink-upstream/`). An
   existing consumer — 3D Slicer, PLUS, any custom integration —
   should compile and link against our shim *unchanged*, substituting
   `-loigtl_compat -loigtl` for `-lOpenIGTLink -ligtlutil`.

Both layers must co-exist: the native API is for new code and must
not carry upstream's legacy design choices (reference-counted intrusive
pointers, synchronous-blocking sockets, factory-registry mutability).
The shim matches upstream exactly and forwards through a sync bridge
to the native transport.

## Non-goals

- ABI compatibility with upstream. API compatibility only; consumers
  recompile. Binary drop-in is out of scope.
- Matching upstream's `#ifdef`-guarded partial features (VideoStreaming,
  OpenIGTLink_PROTOCOL_VERSION ≥ 3 incomplete paths). The shim matches
  what upstream ships in its default build.
- Transport-layer support for the hypothetical v4 tier-3 features
  (stream multiplexing, explicit flow-control frames). The design
  accommodates these without committing to building them.
- Windows-specific code paths at first. macOS + Linux only. Windows
  is a later port — the ASIO backend and TLS are portable; the
  platform-specific code is `SO_REUSEADDR`-style detail.

## Goals (expanded)

From the project-level discussion, plus the dual-layer constraint:

- Async / future-based native API.
- Stateful connection object carrying negotiated capabilities,
  peer identity, TLS session info, and sequence counters. App layer
  queries `conn.capability("<name>")` without knowing framer details.
- Pluggable framer at construction time. Default = v3 58-byte-header
  body-length framing. A second framer (hypothetical v4 chunked or
  multiplexed) is a new class, not a rewrite.
- Opaque message envelope up to dispatch. Transport yields
  `(Header, body_bytes)`; per-message-type parsing is the caller's.
- TLS is a wrapper around `Connection`, not a separate class.
  Certificate-chain info, negotiated cipher, peer identity all flow
  through `conn.capability("tls.peer_cert")` etc.
- **Shim surface.** Every upstream `igtl::` class a deployed
  consumer uses must be available from `igtl/<Upstream>.h` headers
  provided by our `libigtl_compat.a`. Method signatures match
  upstream exactly (including `igtl::` namespace, `Pointer` typedef,
  `New()` factory, `Pack()` / `Unpack()` / `GetPackPointer()` style).
- **Parity test gate.** Upstream's own example programs
  (`Examples/Receiver/ReceiveClient.cxx`, `Examples/Tracker/*.cxx`)
  compile **unchanged** against our shim and run against a live
  peer (either upstream or ourselves) end-to-end.

## Architecture (layered)

```
┌─────────────────────────────────────────────────────────────┐
│  Upstream-compat shim  (libigtl_compat.a)                   │
│    - igtl::ClientSocket, ServerSocket, UDP*Socket           │
│    - igtl::TransformMessage, StatusMessage, ... (aliases)   │
│    - igtl::MessageFactory                                   │
│    - igtl::SmartPointer<T>, LightObject                     │
│  Strict upstream API surface. No new design here.           │
├─────────────────────────────────────────────────────────────┤
│  Sync bridge  (header-only, in core-cpp/transport/sync/)    │
│    - block_on<T>(Future<T>) with timeout                    │
│    - exception→int-return translation                       │
│    - single-threaded I/O runner used by shim calls          │
├─────────────────────────────────────────────────────────────┤
│  Native transport  (libigtl_transport.a, C++17)             │
│    - oigtl::Connection (stateful, capability-bearing)       │
│    - oigtl::Framer (INTERFACE; v3 impl default)             │
│    - oigtl::TlsWrapper (wraps a Connection)                 │
│    - oigtl::Acceptor (server-side; yields Connections)      │
│    - Future<T> / Promise<T>  (or std::future + executor)    │
│  Clean sheet; no upstream-isms leak past this line.         │
├─────────────────────────────────────────────────────────────┤
│  Codec + runtime  (libigtl_runtime.a, libigtl_messages.a)   │
│    — already landed (84 generated message codecs + runtime) │
└─────────────────────────────────────────────────────────────┘
```

**Key invariant:** the native transport does not know the shim
exists. The shim depends on the public transport API. A new consumer
can pick native; a legacy consumer picks the shim. They share the
codec, the sync bridge, and nothing else.

## Native transport contract

A single header `oigtl/transport/connection.hpp` pins the five
properties so every implementation — v3, future v4, test-double, TLS —
honors them:

```cpp
namespace oigtl::transport {

class Connection {
 public:
    // ---- Property 1: stateful, capability-bearing ----
    // Read-only after negotiation. Implementations populate these
    // during connect() / accept() before returning the Connection.
    virtual std::optional<std::string> capability(
        std::string_view key) const = 0;
    virtual std::string peer_address() const = 0;
    virtual std::uint16_t peer_port() const = 0;
    virtual std::uint16_t negotiated_version() const = 0;

    // ---- Property 4: async / future-based ----
    // receive() returns the next framed message on this connection.
    // The Future resolves to (header, body_bytes) — Property 2, opaque.
    // Body bytes are owned by the Future; caller may take ownership.
    struct Incoming {
        runtime::Header header;
        std::vector<std::uint8_t> body;
        // Framer may attach metadata (streaming-chunk index etc.)
        // through a small opaque metadata object.
        std::optional<FramerMetadata> metadata;
    };
    virtual Future<Incoming> receive() = 0;

    // send() takes a fully-packed wire message (header + body). The
    // framer re-frames for the wire — for v3 that's a no-op copy.
    virtual Future<void> send(const std::uint8_t* wire,
                              std::size_t length) = 0;

    // Graceful close; pending receive() resolves with a
    // ConnectionClosedError exception.
    virtual Future<void> close() = 0;

    virtual ~Connection() = default;
};

// ---- Property 3: pluggable framer ----
class Framer {
 public:
    virtual std::optional<Connection::Incoming>
        try_parse(std::vector<std::uint8_t>& buffer) = 0;
    virtual std::vector<std::uint8_t> frame(
        const std::uint8_t* wire, std::size_t length) = 0;
    virtual ~Framer() = default;
};

// Default framer: v3 — 58-byte header + body_size body. Identity
// frame() (wire is already framed). try_parse peels one message off
// the front of the buffer or returns nullopt.
std::unique_ptr<Framer> make_v3_framer();

}  // namespace oigtl::transport
```

And TLS (Property 5):

```cpp
namespace oigtl::transport::tls {

struct Config {
    std::filesystem::path ca_bundle;
    std::optional<std::filesystem::path> client_cert;
    std::optional<std::filesystem::path> client_key;
    bool verify_hostname = true;
    std::optional<std::string> sni_hostname;
    // Future: cipher preferences, session tickets, pinned cert SHA256.
};

// Wraps an existing Connection, returning a new Connection whose
// capability() carries TLS info ("tls.peer_cert.sha256",
// "tls.cipher", "tls.version", ...) and whose send/receive go
// through the TLS session.
std::unique_ptr<Connection> wrap(
    std::unique_ptr<Connection> plain,
    const Config& cfg);

}  // namespace oigtl::transport::tls
```

**Why this satisfies v4 forward compatibility:**

| v4 tier | What lands here |
|---|---|
| Tier 1 (new messages) | Zero transport change. |
| Tier 2 (NGHELLO capabilities) | New keys under `capability()`. Negotiation logic is an app-level helper that runs over `send/receive`. No transport class changes. |
| Tier 3 (streaming / mux) | A new `Framer` implementation. `Connection` is unchanged; construction picks the new framer. |

## Shim surface

The shim lives in `core-cpp/compat/` with its own CMake target:

```
core-cpp/compat/
├── CMakeLists.txt             (produces libigtl_compat.a)
├── include/igtl/              (headers consumers #include)
│   ├── igtlMessageBase.h       ← facade over our generated msg
│   ├── igtlMessageFactory.h    ← upstream API on our registry
│   ├── igtlClientSocket.h      ← sync Connection wrapper
│   ├── igtlServerSocket.h      ← sync Acceptor wrapper
│   ├── igtlSocket.h            ← upstream base class
│   ├── igtlLightObject.h       ← intrusive ref counting
│   ├── igtlSmartPointer.h      ← Pointer<T> = ref-counted
│   ├── igtlTransformMessage.h  ← one header per message type
│   ├── igtlObject.h / igtlObjectFactory.h / ...
│   └── ... (every upstream public header we claim to shim)
└── src/
    ├── MessageBase.cpp          (wraps oigtl::messages::* + PackBuffer)
    ├── ClientSocket.cpp         (sync_bridge::block_on over Connection)
    ├── ServerSocket.cpp
    ├── LightObject.cpp          (ref-count primitives)
    └── ... (one .cpp per header)
```

**Class aliasing strategy.** For every upstream `igtl::FooMessage`
that upstream ships, the shim provides `igtl/igtlFooMessage.h` whose
`igtl::FooMessage` is a class that:

- Holds a value-type `oigtl::messages::FooMessage` internally (our
  generated typed class).
- Maintains an owned buffer for `GetPackPointer()` / `GetPackBodyPointer()`
  returns. Upstream's API gives consumers raw pointers into the
  message's internal storage; we honor that by keeping an
  `std::vector<std::uint8_t>` buffer of the packed wire bytes.
- Implements `Pack()`, `Unpack()`, `AllocatePack()` as thin forwarders
  into our codec + buffer.
- Matches upstream's ref-counted smart-pointer idiom via `LightObject`
  base + `SmartPointer<T>`. **We do NOT use `std::shared_ptr` here** —
  upstream consumers use `itk::Object`-style intrusive refcount,
  and matching the idiom is part of the drop-in claim.

**Error handling.** Upstream returns ints (0/1) and sets bit flags;
we return exceptions. The shim catches `oigtl::error::ProtocolError`
and returns 0 (failure); on success, returns 1 (or the
`MessageHeader::UNPACK_*` bitmask as appropriate). Upstream has
logging side effects (`std::cerr` on errors) — we replicate the
observable ones.

**Behavior compatibility — quirks.** Our codec is stricter than
upstream. Five places diverge:

1. NDARRAY cross-field invariant (ours rejects; upstream accepts).
2. IMAGE cross-field invariant (ours rejects; upstream accepts).
3. COLORT index_type/map_type validity (ours rejects; upstream
   fallthrough accepts).
4. POLYDATA section alignment (ours rejects odd sizes; upstream
   doesn't check).
5. BIND nametable even + bodies sum (ours rejects; upstream doesn't).

For drop-in compat, the shim exposes a `SetStrictMode(bool)` flag
on `igtl::MessageBase` (default: `false`, i.e., upstream-permissive).
In permissive mode, post-unpack invariants are **skipped**. Strict
mode is opt-in for consumers who want our tighter behavior.

Implementation: the codec gains a thread-local
`oigtl::runtime::policy::strict_invariants` flag. The shim toggles
it per-call.

## Phased work

### Phase 1 — Native transport, in-memory loopback (2 days)

**Deliverable:** `libigtl_transport.a` with `Connection`, `Framer`,
`make_v3_framer()`. A *loopback* implementation
(`make_loopback_pair()`) for tests — one writer feeds another
reader through a shared byte buffer. No real I/O.

**Acceptance:**
- Unit tests build both ends of a loopback pair, push 1000 messages
  through, verify byte-for-byte preservation via receive().
- `Future<T>` semantics: await, fulfill, cancel, error propagation
  all exercised.
- `Connection::capability()` returns loopback-specific keys
  (`"loopback.peer_id"`).
- Property 2 check: receive() yields raw `body_bytes`; test asserts
  the bytes match `wire[kHeaderSize:]` for every message.

### Phase 2 — TCP backend via ASIO (standalone) (2 days)

**Deliverable:** `make_tcp_client(host, port)` returning
`Future<std::unique_ptr<Connection>>`; `make_tcp_acceptor(port)`
returning an `Acceptor` yielding `Future<std::unique_ptr<Connection>>`
on each accept. Internal executor: a single `asio::io_context` thread
owned by a library-global singleton (exposed for override).

**Dependency:** standalone ASIO (header-only) vendored under
`core-cpp/thirdparty/asio/`. Not Boost.Asio.

**Acceptance:**
- Integration test: client sends 100 TRANSFORM messages to a local
  server, server echoes them back, client receives them all, byte-
  exact.
- Timeout behavior: `Connection::receive()` with cancellation resolves
  with `OperationCancelled` exception, not a hang.
- Graceful close: FIN from peer → pending receive() resolves with
  `ConnectionClosedError`.

### Phase 3 — TLS wrapper via mbedTLS or OpenSSL (2 days)

**Deliverable:** `tls::wrap(connection, config)` returning a new
`Connection` whose send/receive go through the TLS session.
`capability()` carries `"tls.cipher"`, `"tls.version"`,
`"tls.peer_cert.sha256"`, `"tls.peer_cert.subject"`.

**Library choice:** mbedTLS 3.x. Lighter than OpenSSL, no
platform-specific system-SSL detection, easier vendoring. OpenSSL
is an opt-in build flag if a downstream needs it.

**Acceptance:**
- Integration test with a self-signed cert: client verifies against
  a CA bundle, server presents the cert, `capability("tls.peer_cert.
  sha256")` matches expected.
- Hostname verification works and can be disabled via `Config::
  verify_hostname = false`.
- Negative test: untrusted cert rejected with
  `TlsHandshakeError`.

### Phase 4 — Sync bridge (0.5 days)

**Deliverable:** `oigtl::transport::sync::block_on<T>(Future<T>,
std::chrono::duration timeout)` returning `T` or throwing
`TimeoutError` / the Future's exception. A global I/O thread
pumps the asio::io_context so `block_on` calls from any thread
work. Header-only.

**Acceptance:**
- `block_on(connection->receive(), 1s)` returns a received message
  synchronously.
- `block_on(future_that_never_resolves, 100ms)` throws
  `TimeoutError` at ~100ms.
- Threadsafe: two threads can `block_on` different futures
  concurrently.

### Phase 5 — Shim: LightObject + SmartPointer + factory (1 day)

**Deliverable:** `libigtl_compat.a` with the upstream object model:

- `igtl::LightObject` — base class with `Register()`, `UnRegister()`,
  `GetReferenceCount()`.
- `igtl::SmartPointer<T>` — intrusive ref-counted pointer matching
  upstream's `typedef SmartPointer<T> Pointer`.
- `igtl::Object` — extends LightObject.
- `igtlNewMacro(T)` + `igtlTypeMacro` — macros matching upstream's
  header conventions.

**Acceptance:**
- A `TEST(SmartPointer, RefCount)` exercising `Register`/`UnRegister`
  produces the same behavior as upstream's (same test file cribbed
  from upstream `Testing/`).
- Headers compile under `-std=c++17 -Wall -Wextra -Werror`.

### Phase 6 — Shim: Message facades (2 days)

**Deliverable:** For each upstream message type shipped in the
default factory — about 28 classes including GET_/STT_/STP_/RTS_
variants — a facade class under `igtl/` matching upstream's
signature. Implemented as a small forwarding wrapper around
our `oigtl::messages::*` generated class + a packed-bytes buffer.

**Code-generated wherever possible.** We already have a codegen
pipeline for our generated C++ classes; the shim facades are
deterministic transforms over the same schemas (same field list →
same setters). Add a `codegen cpp-compat` subcommand that emits
the shim headers + sources.

**Acceptance:**
- Upstream's `Testing/igtlTransformMessageTest.cxx` compiles and
  passes against our shim (link `-loigtl_compat`, the rest is
  identical to the upstream test).
- Byte-exact pack output: a message packed via
  `igtl::TransformMessage::Pack()` on the shim produces the exact
  same wire bytes as the upstream test vector, for every of the
  28 types with an upstream test.

### Phase 7 — Shim: Socket classes (1 day)

**Deliverable:**
- `igtl::Socket` — shim base with upstream's method signatures.
- `igtl::ClientSocket` — `ConnectToServer(host, port)` using
  `oigtl::transport::make_tcp_client` under the hood, synchronized
  via `block_on`.
- `igtl::ServerSocket` — `CreateServer(port)` + `WaitForConnection()`
  using our `Acceptor`.
- Receive/Send with the `igtlUint64 Receive(void* data, length,
  bool& timeout, int readFully=1)` signature. Timeout semantics
  match upstream.

**Acceptance:**
- Upstream's `Examples/Receiver/ReceiveClient.cxx` compiles
  *unchanged* against our shim + links against `-loigtl_compat
  -loigtl_transport -loigtl_messages -loigtl_runtime`.
- End-to-end: unmodified upstream Tracker example → our
  ReceiveClient (built against shim) → matches byte-exact against
  upstream ReceiveClient's output for the same stream.

### Phase 8 — Parity test suite (1 day)

**Deliverable:** CI job `shim-parity` that builds every upstream
example program we claim to support (Receiver, Tracker, Imager,
ImageDatabaseServer, Bind, Capability, Point, PolyData,
QuaternionTrackingData, ImageMeta) against our shim, then runs an
integration matrix:

- Row: sender (upstream binary vs shim binary)
- Col: receiver (upstream binary vs shim binary)
- Four cells, every cell must round-trip a scripted message stream
  byte-exact (or semantically exact through the strict-mode flag).

**Acceptance:** all four cells pass in CI. Any drift (a new upstream
field not shimmed, a new example we haven't covered) fails the job.

### Phase 9 — Integration & docs (0.5 days)

**Deliverable:**

- `core-cpp/transport/README.md` — native API quickstart.
- `core-cpp/compat/README.md` — how to migrate from upstream. One-
  page table: old include → new include (identical namespace + class,
  different library).
- Root `README.md` updated: "Wire codec: done. Transport + shim:
  done. V4: designed." (eventually.)
- CMake package: `find_package(oigtl)` exposes `oigtl::transport`,
  `oigtl::compat`, `oigtl::messages` as imported targets.

## Parallel-track: v4 design RFC (concurrent with Phase 1-2)

Not a phase of *this* plan, but to keep the transport decisions
sound, a short v4 RFC lands in parallel with Phase 1-2:

- Define tier: which of (tier 1 = new messages only, tier 2 =
  NGHELLO capabilities, tier 3 = framing change) v4 actually needs.
- Draft NGHELLO message schema.
- Identify capability keys the v4 negotiation will use (e.g.
  `"compression"`, `"max_message_size"`, `"heartbeat_interval_ms"`).

If v4 lands tier 3, Phase 2's `Framer` class is the right hook.
If not, tier 3 plumbing stays speculative and no transport work
changes.

## Estimates

| Phase | Scope | Duration |
|---|---|---|
| 1 | Native + loopback | 2 days |
| 2 | TCP via ASIO | 2 days |
| 3 | TLS wrapper | 2 days |
| 4 | Sync bridge | 0.5 days |
| 5 | LightObject + SmartPointer | 1 day |
| 6 | Message facades | 2 days |
| 7 | Socket shims | 1 day |
| 8 | Parity test matrix | 1 day |
| 9 | Docs + integration | 0.5 days |
| **Total** | | **~12 days** (~2.5 weeks focused) |

## File touch list

**New:**
- `core-cpp/include/oigtl/transport/connection.hpp`
- `core-cpp/include/oigtl/transport/framer.hpp`
- `core-cpp/include/oigtl/transport/tls.hpp`
- `core-cpp/include/oigtl/transport/future.hpp`
- `core-cpp/include/oigtl/transport/sync.hpp`
- `core-cpp/src/transport/connection_loopback.cpp`
- `core-cpp/src/transport/connection_tcp.cpp`
- `core-cpp/src/transport/framer_v3.cpp`
- `core-cpp/src/transport/tls.cpp`
- `core-cpp/tests/transport_*.cpp` (per-phase test files)
- `core-cpp/thirdparty/asio/` (vendored)
- `core-cpp/thirdparty/mbedtls/` (vendored or CMake-fetched)
- `core-cpp/compat/include/igtl/igtl*.h` (~35 upstream-shaped headers)
- `core-cpp/compat/src/*.cpp` (shim impls)
- `core-cpp/compat/CMakeLists.txt`
- `corpus-tools/src/oigtl_corpus_tools/codegen/cpp_compat.py`
  (shim codegen; Phase 6)
- `corpus-tools/src/oigtl_corpus_tools/codegen/templates/cpp_compat_message.hpp.jinja`

**Modified:**
- `core-cpp/CMakeLists.txt` — add transport, compat subprojects;
  export `oigtl::transport` / `oigtl::compat` imported targets.
- `core-cpp/src/runtime/policy.cpp` (new) — thread-local
  `strict_invariants` flag for shim permissive mode.
- `.github/workflows/ci.yml` — new `shim-parity` job.

## Open questions to resolve before Phase 1

1. **Async primitive.** Do we vendor our own `Future<T>` or use
   `std::future` + an executor? Leaning toward a minimal custom
   one — `std::future` lacks cancellation and composability.
   Alternative: folly-style `SemiFuture` / `Task`. Decide before
   Phase 1 or it's expensive to change.

2. **Executor exposure.** Do we give consumers an `asio::io_context&`
   accessor, or hide asio entirely? Hiding keeps us free to swap
   backends (epoll, io_uring) later. Exposing makes composition with
   existing asio apps seamless.

3. **Vendored asio.** Standalone asio is header-only but large.
   Vendor or `find_package(asio)`? Leaning vendor (standalone is
   the only known-good version for our build, and avoids a system-
   dependency footgun).

4. **mbedTLS version.** 3.x is current but API-different from 2.x.
   Pin to a specific tag. Vendored source.

5. **Strict-mode signal in the shim.** Thread-local flag in the
   runtime, or a per-message bool? Thread-local is cleaner (matches
   upstream's "global permissive" default) but interacts poorly with
   async / thread-pool consumers. Decide before Phase 6.

6. **Server-side accept API shape.** `for (auto conn : acceptor)`
   iterator, or `Future<Connection> accept()` loop? The latter is
   more async-friendly; the former is more idiomatic C++. We can
   offer both — iterator over futures — but the primitive decides
   complexity.

## Resuming from cold

Key facts for the next session:

1. **Two libraries, not one.** `libigtl_transport.a` is our design;
   `libigtl_compat.a` is a shim matching upstream. They coexist.
2. **Shim is codegenerated where possible.** The 28 message-facade
   classes come from the same schemas as our typed library; the
   shim just uses upstream's class-name conventions + forwarding
   layer.
3. **Upstream examples are the parity gate.** If Tracker / Receiver /
   Imager compile unchanged against the shim and interoperate with
   upstream, the shim is real. If they don't, it isn't.
4. **Strict-mode is the invariant escape hatch.** Our post_unpack
   invariants (NDARRAY / IMAGE / COLORT / POLYDATA / BIND) don't run
   under the shim by default; they're available for opt-in.
5. **v4 doesn't block this plan.** The `Framer` interface is the
   hook for any v4 framing change; messages ride the existing
   `Connection` API; capability negotiation rides `capability()`.
