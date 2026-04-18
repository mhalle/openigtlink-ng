# `oigtl::Client` — user guide

The ergonomic C++17 client for OpenIGTLink traffic. Typed
send/receive, a dispatch loop for callback-style code, and opt-in
resilience (auto-reconnect, offline buffer, TCP keepalive) for
research-lab deployments on flaky networks.

If you're writing new code, use this. The compat shim
(`igtl::ClientSocket` in `core-cpp/compat/`) exists to keep
unmodified upstream code building against us; new code shouldn't
pay the upstream-API tax.

---

## Minimum viable client

```cpp
#include "oigtl/client.hpp"
#include "oigtl/messages/transform.hpp"

int main() {
    auto client = oigtl::Client::connect("tracker.lab", 18944);

    oigtl::messages::Transform tx;
    tx.matrix = { 1,0,0, 0,1,0, 0,0,1, 0,0,0 };    // column-major 3x4
    client.send(tx);

    auto reply = client.receive<oigtl::messages::Status>();
    std::printf("status code %u: %s\n",
                reply.body.code,
                reply.body.status_message.c_str());
}
```

That's the whole API for a simple point-to-point integration.
Header includes are single-module; linking is `oigtl::oigtl` via
CMake's `find_package(oigtl)`.

---

## Dispatch-loop style

For servers or peers that handle multiple message types, register
per-type handlers and call `run()`:

```cpp
oigtl::Client::connect("tracker.lab", 18944)
    .on<oigtl::messages::Transform>([&](auto& env) {
        renderer.update_pose(env.body.matrix);
    })
    .on<oigtl::messages::Status>([&](auto& env) {
        if (env.body.code != 1) log_error(env.body);
    })
    .on_unknown([&](auto& inc) {
        std::printf("unhandled type_id: %s\n", inc.header.type_id.c_str());
    })
    .on_error([&](auto eptr) {
        try { std::rethrow_exception(eptr); }
        catch (const std::exception& e) { log_error(e.what()); }
    })
    .run();    // blocks; returns on stop(), peer close, or error
```

Handlers run on the thread that called `run()`. A call to
`client.stop()` from any thread wakes the loop at the next
dispatch tick.

---

## Request / response

For protocols where you send a query and expect a reply of a
known type:

```cpp
oigtl::messages::GetStatus req;
auto reply = client.request_response<oigtl::messages::Status>(
    req, std::chrono::seconds(5));
```

Messages of other types that arrive while waiting go through the
registered `on<T>` handlers or get dropped. The call throws
`TimeoutError` if the reply doesn't arrive in time.

---

## Resilient client (research-lab deployments)

If your tracker lives on lab Wi-Fi, the network drops for 10
seconds, and "my code needs to survive that" is table stakes —
turn on the three resilience features. None are on by default;
opt in explicitly.

```cpp
oigtl::ClientOptions opt;
opt.auto_reconnect             = true;    // reconnect on drop
opt.tcp_keepalive              = true;    // detect dead peer fast
opt.offline_buffer_capacity    = 100;     // buffer during outage
opt.offline_overflow_policy    =
    oigtl::ClientOptions::OfflineOverflow::DropOldest;
// Optional tuning:
opt.reconnect_initial_backoff  = std::chrono::milliseconds(200);
opt.reconnect_max_backoff      = std::chrono::seconds(30);
opt.reconnect_backoff_jitter   = 0.25;   // ±25%
opt.reconnect_max_attempts     = 0;      // forever (default)

auto client = oigtl::Client::connect("tracker.lab", 18944, opt);

// Optional lifecycle callbacks — fire on the reconnect worker
// thread. Keep them short; dispatch heavy work to your own pool.
client
    .on_disconnected([&](std::exception_ptr cause) {
        metrics.increment("disconnects");
    })
    .on_connected([&] {
        metrics.increment("reconnects");
    })
    .on_reconnect_failed(
        [&](int attempt, std::chrono::milliseconds next_delay) {
            if (attempt == 5) alert_operator();
        });
```

### What the three features do

**`auto_reconnect`** — after the initial connection succeeds,
the Client spawns a background worker. When the connection
drops, the worker attempts to re-establish with exponential
backoff (200ms → 400ms → 800ms … → 30s cap, with ±25% jitter
to avoid reconnect storms). No limit by default.
`send()` and `receive()` called during the outage don't throw;
they block or buffer per policy.

**`tcp_keepalive`** — enables `SO_KEEPALIVE` with tuned
intervals so a half-dead peer (remote crash, cable yanked, NAT
idle-out) is detected in ~60 seconds instead of hours. Pure
OS-level; no application-layer ping required.

**`offline_buffer_capacity`** — bounded FIFO queue of already-
packed messages. Filled by `send()` calls made while the
connection is down; drained on reconnect before any new sends.
Three overflow policies:

| Policy | `send()` when full | Right for |
|---|---|---|
| `DropOldest` | Succeeds; discards head of queue | Telemetry (pose, sensor) — old data is stale anyway |
| `DropNewest` *(default)* | Throws `BufferOverflowError` | Commands / transactions — surface the problem |
| `Block` | Waits up to `send_timeout` for space | Strictly-ordered flows with bounded throughput |

### Error model under resilience

| Condition | Throws |
|---|---|
| Initial `connect()` fails after retries | `ConnectionClosedError` |
| Drop during a `send()`, `auto_reconnect` off | `ConnectionClosedError` |
| Drop during a `send()`, `auto_reconnect` on, buffer has room | *nothing* — enqueued |
| Drop during a `send()`, `auto_reconnect` on, buffer full, `DropNewest` | `BufferOverflowError` |
| `reconnect_max_attempts` exhausted, subsequent calls | `ConnectionClosedError` (terminal) |
| `receive()` during a drop, `auto_reconnect` on | Blocks until reconnect or `receive_timeout` |

### Thread safety

- `send()` / `receive()` are thread-safe when `auto_reconnect`
  is on (internal mutex serializes).
- `Client` is move-only.
- Callbacks fire on the reconnect worker thread; don't call back
  into `Client` from them without thinking about reentrancy
  (it's permitted but easy to deadlock yourself if a callback
  does something synchronous).

### When NOT to use `auto_reconnect`

- **Adopted connections** — `Client::adopt()` doesn't know how
  to re-dial an externally-owned socket. `auto_reconnect` is
  forced off in that case (silently).
- **Short-lived tools** — a one-shot CLI that queries status
  once and exits has no reason to keep trying.
- **Protocol-dependent state** — if your session has
  authenticated or subscribed to a topic via application-layer
  messages, the reconnected socket is a fresh session. Use
  `on_connected` to re-subscribe.

---

## Escape hatches

```cpp
// Direct access to the current Connection — when you need async
// primitives, capability() strings, or peer address/port. With
// `auto_reconnect`, the returned reference is invalidated when
// the connection drops; prefer send()/receive() for resilient
// configurations.
transport::Connection& c = client.connection();

// Current options (read-only).
const ClientOptions& opt = client.options();

// Close immediately. Also sets stop_requested; a running run()
// loop exits.
client.close();
```

---

## Related docs

- **[CLIENT_TRANSPORT_PLAN.md](CLIENT_TRANSPORT_PLAN.md)** — the
  design plan for the resilience features (what decisions we
  made, why, and what's intentionally deferred). Useful when
  reading the implementation in `src/client.cpp`.
- **[compat/MIGRATION.md](compat/MIGRATION.md)** — mapping from
  upstream `igtl::*` APIs to the modern equivalents, for code
  that needs both.
- **[examples/resilient_client.cpp](examples/resilient_client.cpp)**
  — runnable demo of the resilient Client pattern.
- **Header** — `include/oigtl/client.hpp` has per-field doc
  comments and is the authoritative reference.
