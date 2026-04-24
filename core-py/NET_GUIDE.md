# `oigtl.net` — Python transport for OpenIGTLink

A Python-shaped client and server for the OpenIGTLink wire
protocol. The underlying capability matches the C++ `oigtl::Client`
/ `oigtl::Server` — typed send/receive, dispatch-by-type,
resilience for flaky networks, accept-time restrictions. The
Python API is built researcher-first, not C++-in-Python:

- **asyncio-native**, with a blocking `SyncClient` / `SyncServer`
  wrapper for scripts that don't want to learn asyncio.
- **Stdlib `ipaddress` types everywhere**. No custom `IpRange`
  wrappers where stdlib already has the shape.
- **One function per researcher question.** "What IP should I
  share?" → `interfaces.primary_address()`. "What subnets am I
  on?" → `interfaces.subnets()`.
- **Duration fields use unit-bearing names.** `timeout=2` means
  2 **seconds** (stdlib convention: `socket.settimeout`,
  `asyncio.wait_for`). For milliseconds, pass the `_ms` variant:
  `timeout_ms=500`. `timedelta` always works.

If you're writing a new research script, start here. If you
need API parity across the C++ and Python sides of the same
project, the behaviours match — only the syntax is Python-idiomatic.

---

## Minimum viable client

```python
from oigtl.net import Client
from oigtl.messages import Transform, Status

async with await Client.connect("tracker.lab", 18944) as c:
    await c.send(Transform(matrix=[1,0,0, 0,1,0, 0,0,1, 0,0,0]))
    reply = await c.receive(Status)
    print(reply.body.status_message)
```

That's the whole API for a point-to-point integration.

### Blocking variant

For scripts where asyncio would be ceremony:

```python
from oigtl.net import SyncClient

c = SyncClient.connect("tracker.lab", 18944)
c.send(Transform(matrix=[...]))
reply = c.receive(Status, timeout=2)     # 2 s; or timedelta(seconds=2)
c.close()
```

Same exceptions, same types; internally delegated to the async
implementation running on a shared background loop.

---

## Dispatch-loop style

For peers that handle multiple message types:

```python
from oigtl.net import Client
from oigtl.messages import Transform, Status

c = await Client.connect("tracker.lab", 18944)

@c.on(Transform)
async def _(env):
    renderer.update_pose(env.body.matrix)

@c.on(Status)
async def _(env):
    if env.body.code != 1:
        log.error(env.body.status_message)

@c.on_unknown
async def _(env):
    log.info(f"unhandled type_id: {env.header.type_id}")

await c.run()     # blocks until peer closes or c.close()
```

Handlers run on the asyncio loop that calls `run()`. `c.stop()`
from any task wakes the loop at the next dispatch tick.

---

## Resilient client (flaky networks)

If your tracker lives on lab Wi-Fi and "my code needs to survive
a 10-second drop" is table stakes — turn on the resilience
features. None are on by default; opt in explicitly.

```python
from oigtl.net import Client, ClientOptions, OfflineOverflow

opt = ClientOptions(
    auto_reconnect=True,
    tcp_keepalive=True,
    offline_buffer_capacity=100,
    offline_overflow_policy=OfflineOverflow.DROP_OLDEST,
    # Optional tuning:
    reconnect_initial_backoff_ms=200,        # or reconnect_initial_backoff=0.2 (s)
    reconnect_max_backoff=timedelta(seconds=30),
    reconnect_backoff_jitter=0.25,           # ±25%
    reconnect_max_attempts=0,                # forever (default)
)

c = await Client.connect("tracker.lab", 18944, opt)

@c.on_disconnected
def _(cause):
    metrics.increment("disconnects")

@c.on_connected
def _():
    metrics.increment("reconnects")
```

### What the three features do

**`auto_reconnect`** — on every drop after the initial connect,
a background task retries with exponential backoff (200ms → 400ms
→ ... → 30s cap, ±25% jitter). `send()` / `receive()` during the
outage don't raise; they block or buffer per policy. A background
monitor reads speculatively from the socket so drops are detected
even when nobody's in `receive()`.

**`tcp_keepalive`** — `SO_KEEPALIVE` plus per-platform
TCP_KEEPIDLE/INTVL/CNT knobs so a half-dead peer (remote crash,
cable yanked, NAT idle-out) is detected in ~60 s instead of hours.
OS-level, no application ping.

**`offline_buffer_capacity`** — bounded FIFO of already-packed
wire bytes. Filled by `send()` calls during the outage, drained
in order on reconnect before any new sends. Three policies:

| Policy | `send()` when full | Use for |
|---|---|---|
| `DROP_OLDEST` | Succeeds; discards queue head | Telemetry — stale data is OK |
| `DROP_NEWEST` *(default)* | Raises `BufferOverflowError` | Commands — surface the problem |
| `BLOCK` | Waits up to `send_timeout` for space | Strictly-ordered flows |

### Error model under resilience

| Condition | Raises |
|---|---|
| Initial `connect()` fails | `ConnectionClosedError` / `TimeoutError` |
| Drop during `send()`, `auto_reconnect` off | `ConnectionClosedError` |
| Drop during `send()`, `auto_reconnect` on, buffer has room | *nothing* — enqueued |
| Drop during `send()`, `auto_reconnect` on, buffer full, `DROP_NEWEST` | `BufferOverflowError` |
| `reconnect_max_attempts` exhausted | `ConnectionClosedError` (terminal) |
| `receive()` during drop, `auto_reconnect` on | Blocks until reconnect or `receive_timeout` |

---

## Server with host-level restrictions

The security-motivated use case. Accept only peers on the lab
LAN, cap concurrent connections, kick idle sessions:

```python
from oigtl.net import Server, interfaces
from oigtl.messages import Transform, Status
from datetime import timedelta

server = (await Server.listen(18944)) \
    .restrict_to_local_subnet() \
    .set_max_clients(4) \
    .disconnect_if_silent_for(timedelta(minutes=5))

@server.on(Transform)
async def _(env, peer):
    await peer.send(Status(code=1, sub_code=0,
                           error_name="", status_message="ok"))

await server.serve()    # blocks until server.close()
```

### Restriction builders

All return `self` so they chain. All are additive — calling
`allow()` twice composes the union.

| Builder | Effect |
|---|---|
| `server.allow(spec)` | Add *spec* to the allowed-peer list. Accepts a string (`"10.1.2.0/24"`), stdlib `IPv4Network` / `IPv6Network`, stdlib `IPv4Address` / `IPv6Address`, an `IpRange`, or a list/tuple of any of the above. |
| `server.restrict_to_local_subnet()` | One-liner for `allow(interfaces.subnets())`. Lab-LAN only. |
| `server.restrict_to_this_machine_only()` | One-liner for `allow(interfaces.subnets(include_loopback=True))`. Localhost only — useful for tests. |
| `server.set_max_clients(n)` | Cap simultaneous connections. `0` = unlimited. |
| `server.disconnect_if_silent_for(timeout)` | Close peers idle for *timeout*. Accepts `timedelta` or a number of seconds (matches the rest of the library). |
| `server.set_max_message_size_bytes(n)` | Reject inbound messages with `body_size > n`. Pre-parse DoS defence. |

If a peer fails the allow-list check it's closed before any
handler sees the connection — `on_connected` never fires for
rejected peers.

### Sync server

Same pattern as `SyncClient`:

```python
from oigtl.net import SyncServer

server = SyncServer.listen(18944).restrict_to_local_subnet()

@server.on(Transform)
async def _(env, peer):                  # handlers still async
    await peer.send(Status(code=1, ...))

server.serve()   # blocks
```

Handlers stay async because they run on the event loop; the
sync wrapper only blocks the bookkeeping. Researchers who want
purely-sync handler code can offload with `asyncio.to_thread`
inside the handler.

---

## Interfaces helpers — "what's my IP?"

Five entry points on the `interfaces` module:

```python
from oigtl.net import interfaces

interfaces.primary_address()     # IPv4Address('192.168.1.42')
interfaces.subnets()             # [IPv4Network('192.168.1.0/24')]
interfaces.addresses()           # every non-loopback, non-link-local IP
interfaces.enumerate()           # list[NetworkInterface] (full detail)
```

Defaults hide loopback and link-local because those are rarely
what a researcher asking "what's my IP" means. Opt back in with
`include_loopback=True` / `include_link_local=True`.

`primary_address()` picks the IP you'd paste into a colleague's
config:

- Private (RFC 1918) beats public — lab setups live on LANs.
- IPv4 beats IPv6 unless `family=6` is requested.
- Never loopback or link-local.

---

## Escape hatches

```python
# The underlying stdlib types are the canonical ones — nothing
# extra to learn.
primary = interfaces.primary_address()
assert isinstance(primary, ipaddress.IPv4Address)

# PeerPolicy can be constructed directly when the fluent builders
# aren't flexible enough.
from oigtl.net.policy import PeerPolicy, parse
policy = PeerPolicy(allowed_peers=[parse("10.42.0.0/24")])
server = await Server.listen(18944, ServerOptions(policy=policy))

# Close immediately. Wakes any running serve() / run() loop.
await server.close()
await client.close()
```

---

## Related

- **[examples/resilient_client.py](examples/resilient_client.py)**
  — runnable demo of the resilient Client pattern. Self-contained
  (runs its own server) and self-verifying.
- **[../core-cpp/CLIENT_GUIDE.md](../core-cpp/CLIENT_GUIDE.md)**
  — the C++ Client guide. Same capability, C++ ergonomics.
