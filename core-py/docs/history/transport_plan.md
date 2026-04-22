# core-py transport — plan

> **Status: shipped.** Async + sync TCP client, TCP server,
> WebSocket client and server all live under
> [`core-py/src/oigtl/net/`](src/oigtl/net/) with 149 tests
> including cross-runtime interop against core-cpp and core-ts.
> This document is retained as the original design record. For
> current usage see [`NET_GUIDE.md`](NET_GUIDE.md); for status
> see [`README.md`](README.md).

Originally scoped as a port of the client + server transport
features from core-cpp to Python, with asyncio as the primary
model and a sync wrapper for the research-friendly case.

Target: a Python researcher writing a tracker client or a
receive server gets the same ergonomics as our C++ API, without
having to `import socket` or hand-frame bytes.

---

## Motivation

core-py is codec-only today. A consumer wanting to publish TRANSFORM
at 60 Hz does:

```python
import socket, struct
from oigtl.messages import Transform
from oigtl.runtime.header import pack_header, HEADER_SIZE

sock = socket.socket()
sock.connect(("tracker.lab", 18944))
tx = Transform(matrix=[...])
body = tx.pack()
header = pack_header(version=2, type_id="TRANSFORM",
                     device_name="me", timestamp=...,
                     body=body)
sock.sendall(header + body)
```

…and re-implements reconnect, framing, keepalive, restrictions
themselves. That's the gap this plan closes.

pyigtl already provides `OpenIGTLinkClient` / `OpenIGTLinkServer`
for a tiny subset of the protocol (6 message types, no
restrictions, naïve reconnect). We port our C++ API's shape to
Python with all 83 message types and the full resilience +
restriction surface.

---

## Scope

### In

**Client** — mirrors `oigtl::Client` (see `core-cpp/CLIENT_GUIDE.md`):

- `asyncio`-native as the primary interface.
- Sync wrapper (`Client.connect_sync(...)`) for research scripts
  that don't want to learn asyncio.
- `async with Client.connect(...) as c:` context manager.
- `await c.send(msg)` / `await c.receive(Transform)` / `async for
  msg in c.messages():`.
- Decorator dispatch: `@c.on(Transform)` + `await c.run()`.
- Resilience: `auto_reconnect`, `tcp_keepalive`,
  `offline_buffer_capacity`, `offline_overflow_policy`,
  lifecycle callbacks (`on_connected`, `on_disconnected`,
  `on_reconnect_failed`).
- `ClientOptions` Pydantic model (symmetric with C++'s
  `ClientOptions` struct).

**Server** — mirrors `oigtl::Server` and compat `igtl::ServerSocket`:

- `asyncio.start_server`-backed accept loop.
- Sync wrapper using asyncio under a background thread.
- Per-type dispatch: `@server.on(Transform)` etc.
- Restrictions API (ported verbatim from C++ ergo Server):
  `restrict_to_this_machine_only()`,
  `restrict_to_local_subnet(iface=None)`,
  `allow_peer(ip_or_host)`,
  `allow_peer_range(first, last)`,
  `set_max_simultaneous_clients(n)`,
  `disconnect_if_silent_for(timedelta)`,
  `set_max_message_size_bytes(n)`.
- Lifecycle: `on_connected`, `on_disconnected` callbacks fired
  per-peer.

**Shared** — pulled out of the two entry points:

- `oigtl.transport.framer` — the 58-byte header + body framing
  split (port of `framer_v3.cpp`).
- `oigtl.transport.policy` — `PeerPolicy`, `IpRange`, parse
  helpers (port of `core-cpp/.../transport/policy.hpp`).
- `oigtl.transport.interfaces` — getifaddrs / GetAdaptersAddresses
  via stdlib `socket.if_nameindex` + `netifaces` fallback if
  available. Prefer zero runtime deps.

### Out of scope

- **Noise transport.** Tracked separately in `TRANSPORT_PLAN.md`
  at repo root; ports from C++ when that lands.
- **WebSocket transport.** Future; follows Noise.
- **MQTT gateway / other broker integrations.** Tracked
  architecturally as gateways, not codec features.
- **`pyigtl2` compatibility facade.** A `pyigtl`-shaped import
  for existing code. Low-priority; add if community asks.
- **Zero-copy optimization.** Python's GC model makes this
  painful; we do explicit memoryview-based reads where helpful
  but don't go further.

---

## Design decisions

### asyncio-first, sync as wrapper

Two reasons this is the right order:

1. **Modern Python is asyncio.** aiohttp, FastAPI, asyncpg,
   websockets, aiomqtt — the ecosystem a Python researcher
   integrating with would already be using speaks asyncio. A
   blocking-socket API reads as dated.

2. **The C++ model is naturally async under the hood.** Our
   reconnect worker in C++ is effectively a coroutine that
   waits on a condition variable. In Python it's literally an
   asyncio Task. The sync path is a thin veneer: start an
   event loop on a background thread, submit coroutines via
   `asyncio.run_coroutine_threadsafe`.

The sync wrapper still provides `c.send(msg)` / `c.receive(T)`
semantics, just without `await`, for research scripts. Same
codec on both paths; same resilience logic; same tests.

### Symmetry with C++

Where the Python idiom diverges from C++'s obviously, we let it
diverge and document why. Where it matches, we match exactly
— including type names and field names on `ClientOptions`,
method names on `Server`, exception type names.

Matches:
- `ClientOptions.auto_reconnect`, `.tcp_keepalive`,
  `.offline_buffer_capacity`, etc. — identical field names.
- `on_connected` / `on_disconnected` / `on_reconnect_failed`
  callbacks — same names.
- `BufferOverflowError` exception — same type.
- Server restriction builders — identical names.

Diverges:
- `chrono::milliseconds` → `datetime.timedelta` (or int
  milliseconds? see open question below).
- Callbacks receive Python exceptions, not `exception_ptr`.
- `Envelope[T]` generic dataclass — Python has
  `typing.Generic`, not C++ templates.
- Dispatch via decorator (`@c.on(T)`) — more Pythonic than
  `.on<T>(handler).on<U>(handler)` chaining (though we
  support both).

### Keepalive — same TCP knobs

Port `detail::net_compat::configure_keepalive`:

```python
def configure_keepalive(sock: socket.socket,
                        idle: timedelta,
                        interval: timedelta,
                        count: int) -> None:
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
    if hasattr(socket, "TCP_KEEPIDLE"):           # Linux
        sock.setsockopt(socket.IPPROTO_TCP,
                        socket.TCP_KEEPIDLE,
                        int(idle.total_seconds()))
    elif hasattr(socket, "TCP_KEEPALIVE"):        # macOS
        sock.setsockopt(socket.IPPROTO_TCP,
                        socket.TCP_KEEPALIVE,
                        int(idle.total_seconds()))
    if hasattr(socket, "TCP_KEEPINTVL"):
        sock.setsockopt(...)
    if hasattr(socket, "TCP_KEEPCNT"):
        sock.setsockopt(...)
    # Windows: use socket.ioctl(SIO_KEEPALIVE_VALS, ...).
```

Same per-platform treatment, zero new deps.

### Offline buffer — asyncio.Queue

For asyncio path: a bounded `asyncio.Queue` of packed bytes.
For sync path: a `threading.Condition`-backed equivalent.
Both implement the three overflow policies (DropOldest,
DropNewest, Block) identically to the C++ side.

### Interface enumeration — stdlib where possible

For `restrict_to_local_subnet()`, we need the host's active
interfaces + subnets. Options:

- `psutil.net_if_addrs()` — clean but pulls a heavy dep.
- `socket.if_nameindex()` + manual ioctls — Linux-only, fragile.
- `netifaces` — small C extension, portable, not maintained for
  years.
- Stdlib `socket.getaddrinfo("127.0.1.1", None)` — gets local
  IP but not netmask.

**Decision**: try stdlib first (getaddrinfo for the host IP,
ifaddr library if available for netmasks, psutil as a lazy
fallback behind an extras_require). Import gracefully degrades:
if no interface enumeration library is available,
`restrict_to_local_subnet()` raises `NotImplementedError` with
a clear install hint.

### Restrictions — peer-IP match from asyncio

asyncio's `StreamWriter.get_extra_info("peername")` gives
`(ip, port)`. Match against `PeerPolicy.allowed_peers` (list of
`IpRange`, same semantics as C++). Reject by closing the writer
before any handler sees the connection.

---

## Package layout

```
core-py/src/oigtl/
├── runtime/               (existing — codec primitives)
├── messages/              (existing — 83 generated types)
├── semantic.py            (existing)
├── oracle_cli.py          (existing)
└── net/                   (NEW — this plan)
    ├── __init__.py        (re-exports Client, Server, etc.)
    ├── framer.py          (header + body framing)
    ├── policy.py          (PeerPolicy, IpRange, parse helpers)
    ├── interfaces.py      (getifaddrs-equivalent, platform-agnostic)
    ├── client.py          (async Client + sync wrapper)
    ├── server.py          (async Server + sync wrapper)
    ├── errors.py          (BufferOverflowError, etc.)
    └── _reconnect.py      (background reconnect worker logic)
```

Import surface:

```python
from oigtl.net import Client, Server, ClientOptions, ServerOptions
from oigtl.net.errors import BufferOverflowError, ConnectionClosedError

# async preferred:
async with Client.connect("host", 18944) as c:
    await c.send(tx)
    reply = await c.receive(Status)

# sync ok:
c = Client.connect_sync("host", 18944)
c.send(tx)
reply = c.receive(Status)
c.close()
```

---

## Phased work

### Phase 1 — framer + policy shared primitives (~1 day)

- `oigtl.net.framer` — `pack_message(envelope) -> bytes`,
  `parse_stream(stream_reader) -> AsyncIterator[Incoming]`.
- `oigtl.net.policy` — port `policy.hpp`'s `IpRange`, `PeerPolicy`,
  `parse`, `parse_ip`, `parse_cidr`, `parse_range`.
- `oigtl.net.interfaces` — enumerate interfaces; graceful
  fallback without psutil.

Not user-facing yet; prepares the foundation.

### Phase 2 — async Client (~1.5 days)

- Basic async `Client.connect(host, port, opt) -> Client`.
- `await c.send(msg)` / `await c.receive(T)` / `async for`.
- `async with` context manager.
- `@c.on(Transform)` decorator + `await c.run()`.
- Per-message-type dispatch.

No resilience features yet; point-to-point happy path only.

### Phase 3 — sync Client wrapper (~0.5 days)

- `Client.connect_sync(host, port, opt)` starts an asyncio
  loop in a daemon thread; returns a sync-surfaced handle.
- `c.send(msg)` / `c.receive(T)` / `c.close()` dispatch
  through `asyncio.run_coroutine_threadsafe`.

### Phase 4 — Client resilience (~1.5 days)

- `ClientOptions.auto_reconnect` + background reconnect Task.
- Exponential backoff with jitter (identical math to C++).
- `offline_buffer_capacity` + `OfflineOverflow` enum (three
  policies).
- `tcp_keepalive` plumbing.
- Lifecycle callbacks.
- 4 resilience tests matching C++'s (`test_*_disabled_throws`,
  `test_*_drop_oldest`, `test_*_drop_newest`,
  `test_*_max_attempts_exhaustion`).

### Phase 5 — async Server (~1 day)

- `Server.listen(port, opt)` backed by `asyncio.start_server`.
- Accept loop → per-peer asyncio Task running handler dispatch.
- `@server.on(Transform)` decorator + `await server.serve()`.
- `on_connected` / `on_disconnected` per-peer callbacks.

### Phase 6 — Server restrictions (~1 day)

- Port the 7 restriction builders from `oigtl::Server`
  (`restrict_to_this_machine_only` etc.).
- Peer-IP matching against `PeerPolicy.allowed_peers` at
  accept time.
- `set_max_simultaneous_clients` — refuse-then-close over cap.
- `disconnect_if_silent_for` — per-peer asyncio timer.
- `set_max_message_size_bytes` — framer check.
- Test parity with C++'s `compat_server_restrictions_test`.

### Phase 7 — sync Server wrapper (~0.5 days)

Same shape as Client's sync wrapper. `Server.listen_sync(...)`.

### Phase 8 — integration tests + docs (~1 day)

- End-to-end test: core-py client ↔ core-cpp server and
  vice versa. Proves wire-level cross-port.
- Port of `resilient_client.cpp` demo to Python:
  `core-py/examples/resilient_client.py`.
- `CLIENT_GUIDE.md` + `SERVER_GUIDE.md` — user-facing docs
  mirroring the C++ equivalents.
- `core-py/README.md` entry for `oigtl.net`.

**Total estimate: ~7–8 days of focused work.**

---

## Acceptance criteria

- [ ] All 83 message types can round-trip over a Python-to-Python
      loopback connection using the new API.
- [ ] Python client → C++ server and C++ client → Python server
      both work for TRANSFORM + STATUS + STRING + POSITION +
      POINT + SENSOR (the stress-test set).
- [ ] 4 new Client resilience tests pass on the
      `ubuntu-latest / macOS / windows-latest` matrix.
- [ ] Server restrictions test parity with the C++ shim version.
- [ ] `resilient_client.py` example runs and self-checks like
      the C++ version.
- [ ] No new required runtime dependencies (psutil optional via
      `[net]` extras).
- [ ] CI matrix entry for the Python transport tests
      (should fall out of the existing `python` job).

---

## Decisions locked in (confirmed)

- **Duration fields accept both `timedelta` and `int` milliseconds.**
  The Pydantic model stores `timedelta` internally; a validator
  coerces ints to `timedelta(milliseconds=n)`. Lets casual code
  pass `tcp_keepalive_idle=30000` while careful code writes
  `tcp_keepalive_idle=timedelta(seconds=30)`. No ambiguity —
  ints are always ms, the docstring is explicit.
- **No `pyigtl2` compat facade in the initial release.** Ship
  `oigtl.net` cleanly; revisit a drop-in facade if the
  community asks. Saves ~100 LoC and a separate naming +
  import story we'd commit to indefinitely.
- **Both sync and async APIs ship together**, per the C++
  experience — asyncio primary, sync wrapper over it. Matches
  the Python ecosystem bifurcation (asyncio-native vs "just
  give me a socket handle"). No language-level reason to pick
  one.

## Open questions for implementation time

1. **Decorator vs chaining for dispatch.** Offer both? Chaining
   is C++-ish, decorator is Pythonic. Probably offer both — the
   decorator calls the chain form internally.

2. **Sync-wrapper lifetime.** A sync `Client` starts a daemon
   thread for its event loop. What happens when the user's
   `__main__` exits but the sync Client was never explicitly
   closed? asyncio cleanup is tricky. `atexit` hook that kills
   the loop?

3. **Server dispatch concurrency.** Each accepted peer gets its
   own asyncio Task. Do handlers run concurrently across peers?
   Yes by default (asyncio doesn't serialize). Do we need a
   per-handler mutex option for researchers who expect FIFO
   semantics?

4. **Sync-client ergonomics for `receive`**. In asyncio,
   `async for` is idiomatic for message streams. In sync, a
   `for msg in c.messages(timeout=N):` generator is natural.
   Decide on the sync API surface for this.

---

## Comparison: before vs after

### Today: researcher writing a minimal tracker client

```python
import socket, struct
from oigtl.messages import Transform
from oigtl.runtime.header import pack_header

sock = socket.socket()
sock.connect(("tracker.lab", 18944))
while True:
    tx = read_from_imu()
    body = Transform(matrix=tx).pack()
    header = pack_header(version=2, type_id="TRANSFORM",
                        device_name="me", timestamp=now_igtl(),
                        body=body)
    sock.sendall(header + body)
    # Network drops? Re-run from scratch.
```

### After phase 3 (sync, happy path)

```python
from oigtl.net import Client
from oigtl.messages import Transform

c = Client.connect_sync("tracker.lab", 18944)
while True:
    c.send(Transform(matrix=read_from_imu()))
```

### After phase 4 (resilience)

```python
from oigtl.net import Client, ClientOptions
from oigtl.messages import Transform

c = Client.connect_sync("tracker.lab", 18944,
    ClientOptions(
        auto_reconnect=True,
        tcp_keepalive=True,
        offline_buffer_capacity=100,
    ))
while True:
    c.send(Transform(matrix=read_from_imu()))   # survives outages
```

### After phase 5–6 (async + server)

```python
import asyncio
from oigtl.net import Server
from oigtl.messages import Transform, Status

server = Server.listen(18944).restrict_to_local_subnet()

@server.on(Transform)
async def on_tx(env, peer):
    await peer.send(Status(code=1, ...))

asyncio.run(server.serve())
```

---

## Relationship to future work

- **core-ts transport** (separate plan): the asyncio-first
  design here translates cleanly to TypeScript's Promise/
  EventTarget model. The SHAPE should match; the IMPL is
  language-native.
- **Noise transport**: once the C++ Noise work lands, port
  to Python (~2–3 days including key management). Same API
  shape, `ClientOptions.security = Noise(pubkey=...)`.
- **WebSocket transport**: after Noise; browser-friendly.
  Python side uses `websockets` library.

Tracked at repo root in `TRANSPORT_PLAN.md` (once that gets
updated — currently mostly about Noise).

---

## Relationship to pyigtl

pyigtl is alive-but-dormant (last feature commit 2022-12-01;
see the investigation in `core-cpp/CLIENT_GUIDE.md` comment
thread). Our `oigtl.net` is a superset of pyigtl's surface:

| pyigtl | `oigtl.net` equivalent |
|---|---|
| `pyigtl.OpenIGTLinkClient(host, port)` | `Client.connect_sync(host, port)` |
| `pyigtl.OpenIGTLinkServer(port)` | `Server.listen_sync(port)` |
| `send_message(msg)` | `c.send(msg)` |
| `wait_for_message(device_name, timeout)` | `c.receive(T, timeout)` (plus device-name filter option TBD) |
| `get_latest_messages()` | `async for msg in c.messages():` |
| (no equivalent) | `auto_reconnect`, `tcp_keepalive`, `offline_buffer_capacity` |
| (no equivalent) | `Server.restrict_to_local_subnet()` etc. |
| 6 message types | 83 message types |

An optional `pyigtl2` compat facade would make drop-in
replacement possible; flagged as decision 4 above.

---

Ready for implementation. Major decisions locked in above; the
four remaining open questions are implementation-time choices
that don't affect the public API.
