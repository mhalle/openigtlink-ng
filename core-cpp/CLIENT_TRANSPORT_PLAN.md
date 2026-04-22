# oigtl::Client transport features — tier-1 plan

> **Status: shipped.** All three features (auto-reconnect, TCP
> keepalive, offline outgoing buffer) are implemented in
> `core-cpp/include/oigtl/client.hpp` and documented in
> [`CLIENT_GUIDE.md`](CLIENT_GUIDE.md). This document is
> retained as the original design record. For current usage,
> see the guide; for overall status, see
> [`README.md`](README.md).

Originally scoped as three features that close the biggest
usability gaps against pyigtl and real-world research-lab
deployments: auto-reconnect, keepalive, and outgoing buffering
while disconnected.

Targeted core-cpp first. Python and TypeScript now ship the
same shape (see their per-core READMEs); core-c stays
codec-only.

## Motivation

Concrete scenarios this plan makes simple:

- **Tracker on lab Wi-Fi.** Signal drops for 15 seconds. Today:
  `client.send()` throws; caller has to `catch`, sleep, rebuild
  the `Client`, re-attach handlers, retry. With this plan:
  `send()` buffers the messages, the background reconnect
  restores the connection, buffered messages drain in order,
  the caller sees an `on_disconnected` + `on_connected` callback
  pair but no exception.
- **Tracker on half-dead TCP.** Remote host crashed without
  sending FIN. Today: reads block forever, writes succeed into
  the kernel buffer until it fills, then block. With this plan:
  `SO_KEEPALIVE` with tuned intervals forces an RST within ~60
  seconds; the connection is marked down; reconnect kicks in.
- **Startup race.** Server isn't up yet when the client launches.
  Today: `connect()` throws after 3 retries × 500ms. With this
  plan: retry indefinitely with exponential backoff, or with a
  caller-specified cap.

## Scope

### In

1. **Auto-reconnect** on connection loss, with exponential
   backoff + jitter. Opt-in via `ClientOptions.auto_reconnect`.
2. **TCP keepalive** (SO_KEEPALIVE + platform-appropriate tuning).
   Opt-in via `ClientOptions.tcp_keepalive`.
3. **Outgoing message buffer** while disconnected, with bounded
   size + configurable overflow policy. Opt-in.
4. **Lifecycle callbacks** — `on_connected`, `on_disconnected`,
   `on_reconnect_failed`. Called on the reconnect worker
   thread.
5. **Tests** that induce connection drops and verify the buffer
   drains in order, callbacks fire, backoff respects the cap.

### Out of scope

- **Application-level keepalive** (periodic GET_STATUS ping).
  Deferred — TCP keepalive covers the half-dead-peer case with
  zero peer cooperation.
- **Per-device latest-only cache** (pyigtl-style). Useful for
  dashboards; defer to a separate `Client::on<T>(latest_only)`
  follow-up.
- **Session resumption** (SEQ / ack / replay). Not in the wire
  spec; would require v4 protocol work.
- **Port to core-py / core-ts / core-c.** Parallel work, tracked
  separately.

## API shape

All additions are on `ClientOptions`:

```cpp
struct ClientOptions {
    // …existing fields (connect_timeout, send_timeout, etc.)…

    // Auto-reconnect ------------------------------------------

    // Default false for back-compat. When true, a connection
    // drop triggers a background reconnect attempt. send() and
    // receive<T>() buffer / block rather than throwing
    // ConnectionClosedError as they do today.
    bool auto_reconnect = false;

    // Starting backoff between reconnect attempts. Doubled on
    // each consecutive failure up to `reconnect_max_backoff`.
    std::chrono::milliseconds reconnect_initial_backoff{200};

    // Ceiling for the exponential backoff sequence.
    std::chrono::milliseconds reconnect_max_backoff{30'000};

    // ± jitter fraction applied to each computed backoff.
    // 0.25 means "multiply the computed backoff by a random
    // value in [0.75, 1.25]". Prevents synchronized reconnect
    // storms when many clients are affected by the same outage.
    double reconnect_backoff_jitter = 0.25;

    // 0 = reconnect indefinitely. Positive = give up after this
    // many consecutive failures; on_reconnect_failed() fires,
    // and subsequent send()/receive() throw terminally.
    int reconnect_max_attempts = 0;

    // TCP keepalive -------------------------------------------

    // When true, enable SO_KEEPALIVE on the client socket.
    // Per-platform TCP_KEEPIDLE/INTVL/CNT defaults (or macOS
    // TCP_KEEPALIVE / Windows SIO_KEEPALIVE_VALS) are tuned to
    // detect dead peers in roughly `tcp_keepalive_idle +
    // tcp_keepalive_count * tcp_keepalive_interval`.
    bool tcp_keepalive = false;
    std::chrono::seconds tcp_keepalive_idle{30};
    std::chrono::seconds tcp_keepalive_interval{10};
    int tcp_keepalive_count = 3;

    // Offline buffer ------------------------------------------

    // 0 = disabled (current behavior: send() throws when down).
    // Positive = at most this many queued messages while
    // disconnected. On reconnect, the queue drains in FIFO
    // order before new sends.
    std::size_t offline_buffer_capacity = 0;

    // When the buffer is full AND auto_reconnect is active:
    //   DropOldest  — discard the head, append the new message.
    //   DropNewest  — keep existing queue; the new send() throws
    //                 BufferOverflowError (new exception type).
    //   Block       — send() blocks until either the buffer
    //                 drains space or the send_timeout elapses.
    enum class OfflineOverflow { DropOldest, DropNewest, Block };
    OfflineOverflow offline_overflow_policy = OfflineOverflow::DropNewest;
};
```

Callbacks added to `Client`:

```cpp
Client& on_connected    (std::function<void()> h);
Client& on_disconnected (std::function<void(std::exception_ptr)> h);
Client& on_reconnect_failed(
    std::function<void(int attempt,
                       std::chrono::milliseconds next_delay)> h);
```

Invoked on the reconnect worker thread. Handlers must be
re-entrant-safe and brief; document that long-running work
should dispatch to the user's own thread pool.

## Behavioral semantics

### Reconnect identity

A reconnecting client uses the **same** (host, port, ClientOptions)
it was originally constructed with. No fallback to alternate
addresses; no DNS re-resolution change of identity (but DNS IS
re-resolved on each attempt — caller wanting fixed-IP behavior
supplies the IP directly). The `Envelope.device_name` default
stays the same; a server that tracks peers by device_name sees
the reconnected peer as the same logical entity.

### Call semantics during disconnection

- `send()` — if `auto_reconnect` is on and `offline_buffer_capacity
  > 0`, enqueue. If enqueue fails (overflow + DropNewest), throw
  `BufferOverflowError`. If `auto_reconnect` is on but buffer is
  disabled, throw immediately (no change from today).
- `receive<T>()` — blocks until a message arrives OR the user's
  `receive_timeout` elapses OR all reconnect attempts are
  exhausted. It does NOT time out on a single disconnection if
  reconnect is in progress.
- `request_response()` — same as `receive<T>()` on the waiting
  side; the send side follows `send()` semantics. A reply
  arriving after a disconnect-reconnect cycle still matches
  by type and device_name, consistent with the stateless
  protocol.

### Ordering guarantees

- Buffered messages drain in FIFO order on reconnect.
- No at-least-once / at-most-once claim: the wire protocol has no
  sequence numbers or acks. A message flushed from our buffer to
  the kernel's TCP buffer immediately before the TCP connection
  drops is lost, with no indication to us. Callers needing strong
  delivery guarantees should emit explicit STATUS acks at the
  application level.
- `on_disconnected` fires before any reconnect attempt. The
  exception_ptr is the one that caused the drop (kept for
  diagnostics).

## Threading model

```
┌──────────────────────────────────────────────────────┐
│ Client                                                │
│                                                       │
│  ┌──────────────────┐    ┌────────────────────────┐   │
│  │ User thread(s)   │    │ Reconnect worker       │   │
│  │                  │    │ (bg thread)            │   │
│  │ send()           │    │                        │   │
│  │ receive<T>()     │    │ - monitors conn state  │   │
│  │ request_resp()   │    │ - attempts reconnect   │   │
│  │                  │    │ - drains offline queue │   │
│  │ lock + state ops │    │ - fires callbacks      │   │
│  └────────┬─────────┘    └─────────┬──────────────┘   │
│           │                        │                  │
│           └────┐              ┌────┘                  │
│                ▼              ▼                       │
│         ┌──────────────────────────┐                  │
│         │ mutable client state     │                  │
│         │ - current Connection     │                  │
│         │ - offline queue          │                  │
│         │ - reconnect attempt count│                  │
│         │ - callbacks              │                  │
│         └──────────────────────────┘                  │
│                                                       │
└──────────────────────────────────────────────────────┘
```

### Lock strategy

Single `std::mutex` protects:
- The `std::unique_ptr<Connection>` (re-assigned on reconnect)
- The offline queue (std::deque<std::vector<uint8_t>>)
- Reconnect attempt counter and backoff state

`send()` takes the lock briefly: either push to the live
connection (handing off to async send) or enqueue. Never
sends over the network while holding the lock.

`receive<T>()` takes the lock briefly: either read from the
current connection or wait on a condition_variable for
reconnect to complete.

Reconnect worker takes the lock to swap in the new connection
and drain the queue; releases it while actually performing the
connect() and send() system calls.

### Move / copy

Move: allowed when the reconnect worker isn't running. With
auto_reconnect on, `Client` is effectively pinned — we make it
move-only with a `std::shared_ptr` to the internal state to
allow the worker thread to outlive a moved-from shell if
needed. Alternative: delete the move when auto_reconnect is
active. Decision at implementation time based on what feels
less surprising.

Copy: deleted, as today.

## Error model

| Condition | Current | Planned |
|---|---|---|
| Connect fails at startup | `ConnectionClosedError` after 3 retries | If `auto_reconnect`: retry indefinitely (or until `reconnect_max_attempts`); else: unchanged. |
| Peer drops mid-session | `ConnectionClosedError` on next send/receive | If `auto_reconnect`: `on_disconnected()` + reconnect; calls buffer/block. |
| Buffer full (DropNewest) | n/a | `BufferOverflowError` (new type derived from `ConnectionClosedError` for source-compat). |
| All reconnects exhausted | n/a | `on_reconnect_failed()`; subsequent calls throw `ConnectionClosedError`. |
| Peer rejected by `PeerPolicy` | `ConnectionClosedError` | Unchanged; no auto-reconnect for authorization failures (distinguishable because connect() returns success then the peer immediately FIN-closes; we detect this via counting consecutive connect-then-drop cycles and stop). |

## Keepalive implementation

### Platform-specific knob mapping

| Platform | SO_KEEPALIVE | Idle interval | Probe count |
|---|---|---|---|
| Linux | setsockopt | `TCP_KEEPIDLE` | `TCP_KEEPCNT` |
| macOS | setsockopt | `TCP_KEEPALIVE` (seconds) | `TCP_KEEPCNT` |
| Windows | setsockopt | `SIO_KEEPALIVE_VALS` ioctl | encoded in same struct |

All three wrapped behind the existing `detail::net_compat`
abstraction. New function:

```cpp
namespace oigtl::transport::detail {
void configure_keepalive(socket_t s,
                         std::chrono::seconds idle,
                         std::chrono::seconds interval,
                         int count);
}
```

POSIX impl uses the Linux/BSD setsockopt chain. Windows impl
uses `WSAIoctl(SIO_KEEPALIVE_VALS, ...)`. Both return void;
setsockopt failures are non-fatal and logged.

### Why not application-level keepalive

Three reasons to stay TCP-only for v1:
- **Zero peer cooperation.** The peer doesn't need to know
  we're keeping the connection alive; the kernel handles it.
- **No wire pollution.** Application-level pings would show
  up in protocol traces and confuse operators reading them.
- **Consistent cross-port story.** Python / TS / C can all
  pass through TCP keepalive config without needing a
  protocol-level keepalive message spec.

Application-level keepalive is fine as a follow-up when we
have a concrete use case (e.g. proxy that terminates TCP but
passes through the application layer).

## Offline buffer implementation

Buffer holds **already-packed bytes**, not typed Envelopes. The
framer's `framing::frame_message()` converts the Envelope to bytes
at enqueue time; the reconnect worker just does a `send()` of
each queue entry without re-traversing the typed layer.

Rationale: the message type + version + metadata are fixed the
moment `send()` was called. Buffering bytes means the message
the user "sent" is the message that eventually hits the wire,
even if the Client's default_version changes via some
reconfiguration (which we don't actually support, but this
guards against similar future drift).

```cpp
struct OfflineEntry {
    std::vector<std::uint8_t> bytes;
    std::chrono::steady_clock::time_point enqueued_at;  // for metrics
};

std::deque<OfflineEntry> offline_queue_;
```

Overflow handling — the three policies:

- **DropOldest**: `pop_front(); push_back(new)`. No exception;
  caller sees their recent send accepted, old messages silently
  lost. Right default for telemetry streams.
- **DropNewest**: if `queue.size() >= cap`, throw
  `BufferOverflowError`. Caller can catch and decide (retry
  later, log, degrade). Right default for command/response.
- **Block**: `send()` waits on a condvar. On reconnect the
  queue drains and signals; if the wait exceeds `send_timeout`,
  throw the usual `TimeoutError`. Safest for transactional use.

Default: `DropNewest` — it surfaces problems explicitly rather
than silently. Telemetry-heavy callers should pick
`DropOldest`. A drop-log hook can be added later if needed.

## Tests

### Unit tests

1. **Auto-reconnect happy path** — start server, connect client
   with `auto_reconnect=true`, stop server mid-session, restart,
   verify `on_disconnected` + `on_connected` fire, buffered
   messages drain.
2. **Exponential backoff timing** — mock clock or tight bounds
   on `steady_clock`; verify successive attempts respect
   `2^n * initial` up to `max_backoff` with jitter within
   spec.
3. **Max-attempts exhaustion** — set `reconnect_max_attempts=3`,
   point at a closed port, verify exactly 3 attempts + final
   `on_reconnect_failed`.
4. **TCP keepalive smoke** — set absurdly-low values (idle=1s,
   count=2, interval=1s), simulate peer crash via SIGKILL-like
   server shutdown that skips FIN, verify the client detects
   within ~4 s.
5. **Buffer DropOldest** — cap=5, send 10 while disconnected,
   verify last 5 arrive on reconnect.
6. **Buffer DropNewest** — cap=5, send 6th, verify
   `BufferOverflowError`.
7. **Buffer Block** — cap=5, send 6th from thread A, thread B
   triggers reconnect via restarting the server; verify thread
   A's send returns successfully.

### Integration test

One end-to-end test exercising the research-lab case: a
Client with `auto_reconnect=true`, `tcp_keepalive=true`,
`offline_buffer_capacity=50` streams 200 TRANSFORM messages
at 60 Hz through a server that restarts twice during the run.
Assert all 200 messages arrive in order.

### Fuzz

Not adding a new libFuzzer target in this round — the new
code is transport-level (sockets, threading) not codec, and
libFuzzer isn't the right tool for the class of bugs that
appears here (races, deadlocks, backoff math). ThreadSanitizer
under normal ctest is the right analog. Add a `-DOIGTL_TSAN=ON`
option that compiles tests under TSan and runs the integration
test; catches races without needing fuzzing.

## Relationship to core-py / core-ts / core-c

Not in this plan. The Python transport is a separate project
(tracker in `core-py/PLAN.md` gains a "transport" section when
we do it). The C++ implementation here is the reference; when
we port, the API shape is negotiable but the semantics (backoff,
buffer policy, callback ordering) should match so operators
moving between languages see identical behavior.

core-c stays codec-only. Auto-reconnect on MCU is a caller
concern; they own their socket loop.

## Open questions / decisions to make at implementation time

1. **`Client` move semantics with auto_reconnect active.** Cleanest
   option: internal `shared_ptr<State>` + join the worker in the
   last owner's dtor. Alternative: delete move when auto_reconnect
   is on. Decide based on which API feels less surprising to
   callers.

2. **DNS re-resolution on every reconnect.** Right default: yes,
   so clients handle a DNS-IP-change scenario (e.g. tracker
   moves between IPs after a restart). Cost: one syscall per
   reconnect. Caller can pin by passing a literal IP.

3. **Default `auto_reconnect`?** Leaning false (explicit opt-in)
   for back-compat. Migration doc points users at the flag.

4. **Buffer capacity default when `auto_reconnect=true`?** Zero
   (explicit opt-in) — users should think about what they want
   to happen to dropped messages. A silent default of 100 with
   DropNewest would cover pyigtl parity but might surprise
   careful users.

5. **`on_disconnected` exception_ptr lifetime.** Passed by value
   (it's a shared_ptr internally); callback can hold onto it
   safely.

## Acceptance

- [ ] All seven unit tests pass.
- [ ] Integration test (200 TRANSFORM / 60 Hz / 2 server
      restarts) delivers all messages.
- [ ] TSan build clean under the integration test.
- [ ] Existing ergo tests (ergo_test.cpp) still pass —
      `auto_reconnect=false` by default means zero behavioral
      change for current callers.
- [ ] `MIGRATION.md` grows a "Client resilience" section with
      the new options and worked examples.

## Estimated scope

- Platform keepalive configurator in `detail::net_compat`: ~1 hour
- Reconnect worker + state machine in `Client`: ~half day
- Offline buffer + overflow policies: ~2 hours
- Callbacks + thread-safety plumbing: ~1 hour
- Seven unit tests + integration test: ~half day
- TSan option + CI gating: ~1 hour
- Docs: ~1 hour

**Total estimate: ~1.5 days of focused work.** Slightly more with
the usual CI-round whack-a-mole.

---

Ready for feedback on the shape before any code lands. Decisions
4 (buffer default) and 5 (exception_ptr lifetime) are the two I'd
most want confirmed before implementation.
