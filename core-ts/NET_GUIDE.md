# `@openigtlink/core` — TypeScript transport guide

The network layer of `@openigtlink/core` covers four environments
out of the same package:

| Want… | Use | Import from |
|---|---|---|
| Node app, plain TCP | `Client` / `Server` | `@openigtlink/core/net` |
| Node app, WebSocket | `WsClient` / `WsServer` | `@openigtlink/core/net` |
| Browser app | `WsClient` only | `@openigtlink/core/net/ws` |
| Bun / Deno | same as Node | `@openigtlink/core/net` |

For per-message field details, see
[`../spec/MESSAGES.md`](../spec/MESSAGES.md). For the package's
structural overview (codec / messages / net layers), see
[`API.md`](API.md). This document is the task-oriented reference
for the network surface specifically.

---

## TCP client (Node)

The minimum viable client:

```ts
import { Client } from "@openigtlink/core/net";
import { Transform, Status } from "@openigtlink/core/messages";

const c = await Client.connect("tracker.lab", 18944);
await c.send(new Transform({
  matrix: [1, 0, 0,
           0, 1, 0,
           0, 0, 1,
           0, 0, 0],
}));

const reply = await c.receive(Status);
console.log(reply.body.statusMessage);

await c.close();
```

`Client.connect(host, port, options?)` resolves to a ready,
fully-connected client or throws — never returns a half-open
handle. `connectTimeoutMs` (default 10 000 ms) bounds the dial.

### Receiving by type

`c.receive(MessageClass)` waits for the next message of the
requested type, dropping any other types received in the
meantime. Returns an `Envelope<T>` with `.header` and `.body`.

```ts
const env = await c.receive(Status, { timeoutMs: 2000 });
//    ^^^ Envelope<Status>
console.log(env.header.deviceName, env.body.code);
```

`c.receiveAny()` returns the next message of any registered
type, no filtering:

```ts
const env = await c.receiveAny({ timeoutMs: 2000 });
//    ^^^ Envelope<unknown>
if (env.body instanceof Status) { /* ... */ }
```

If a `timeoutMs` is given and elapses, both raise
`TransportTimeoutError`.

### Dispatch loop

For peers that handle multiple message types, register handlers:

```ts
import { Client } from "@openigtlink/core/net";
import { Transform, Status, Image } from "@openigtlink/core/messages";

const c = await Client.connect("tracker.lab", 18944);

c.on(Transform, async (env) => {
  renderer.updatePose(env.body.matrix);
});

c.on(Image, async (env) => {
  display.show(env.body);
});

c.on(Status, async (env) => {
  console.log("status:", env.body.statusMessage);
});

c.onUnknown((env) => {
  console.warn("unhandled type", env.header.typeId);
});

c.onError((err) => {
  console.error("transport error", err);
});

// Handlers fire as messages arrive. The client runs its own
// receive loop in the background; you don't manage it.
```

Handlers receive a typed `Envelope<T>` for `.on(T)` and a
`RawBody`-bodied envelope for `.onUnknown(...)`.

`.on(T, handler)` and `.receive(T)` should not be combined for
the same `T` — pick one style per type.

---

## TCP server (Node)

```ts
import { Server } from "@openigtlink/core/net";
import { Transform, Status } from "@openigtlink/core/messages";

const srv = await Server.listen(18944);
console.log("listening on", srv.port);  // useful when you passed 0

srv.onPeerConnected((peer) => {
  console.log("client connected from", peer.remoteAddress);
});

srv.on(Transform, async (env, peer) => {
  // env.body is a Transform; peer is the originating Peer.
  await peer.send(new Status({
    code: 1,
    statusMessage: "ack",
  }));
});

srv.onUnknown(async (env, peer) => {
  console.warn(peer.remoteAddress, "sent unknown type", env.header.typeId);
});

// Server runs until close() is called.
process.on("SIGINT", () => { void srv.close(); });
```

`Server.listen(port, opts?)` binds to `port` (use `0` for an
OS-assigned random port) and resolves once the listening socket
is ready. Read `srv.port` afterwards to learn the actual port.

Each handler receives `(envelope, peer)`. The `Peer` object
exposes `peer.send(message)`, `peer.remoteAddress`,
`peer.remotePort`, and `peer.close()`. A server can have many
peers concurrently; each peer has its own receive loop.

`srv.close()` stops accepting new peers and tears down existing
ones gracefully.

---

## WebSocket client

The same shape as `Client`, but addressed by URL:

```ts
import { WsClient } from "@openigtlink/core/net";   // Node
// or
import { WsClient } from "@openigtlink/core/net/ws"; // browser
import { Transform, Status } from "@openigtlink/core/messages";

const c = await WsClient.connect("wss://tracker.lab/igtl");
await c.send(new Transform({ matrix: [/* … */] }));
const reply = await c.receive(Status, { timeoutMs: 5000 });
await c.close();
```

URL must start with `ws://` or `wss://`. `WsClient` accepts the
same `ClientOptions` as `Client` (timeouts + max message size),
plus a `webSocket` option to inject a custom `WebSocket`
constructor (useful when targeting a non-standard runtime or
swapping in a polyfill for tests). On Node, the default
constructor is the `ws` package; in the browser, it's the
platform global.

### Browser bundle

A bundler targeting the browser should import only from
`@openigtlink/core/net/ws` — the `/net` barrel pulls in Node's
`net` module for `Client` / `Server` even if you don't use them,
which most bundlers will refuse on the browser target.

```ts
// Browser-safe — no Node imports anywhere in this graph.
import { WsClient } from "@openigtlink/core/net/ws";
import { Transform, Status } from "@openigtlink/core/messages";

const c = await WsClient.connect("wss://tracker.lab/igtl");
// ...
```

The `/net/ws` subset re-exports the codec helpers, registry, and
WebSocket classes — everything you can use in a browser without
Node built-ins.

---

## WebSocket server (Node)

```ts
import { WsServer } from "@openigtlink/core/net";
import { Transform, Status } from "@openigtlink/core/messages";

const srv = await WsServer.listen(18944);

srv.onPeerConnected((peer) => {
  console.log("ws peer:", peer.remoteAddress);
});

srv.on(Transform, async (env, peer) => {
  await peer.send(new Status({ code: 1, statusMessage: "ack" }));
});

await new Promise<void>((resolve) => {
  process.on("SIGINT", () => srv.close().then(resolve));
});
```

Same surface as the TCP `Server` — `on`, `onUnknown`,
`onPeerConnected`, `close`. WebSocket-specific: peer connections
go through an HTTP-upgrade handshake before the IGTL frames
start; the framing is identical to TCP once the WebSocket session
is open.

`WsServer` is Node-only because the browser doesn't expose
listening sockets. Use it as a bridge: a Node `WsServer` can
front a TCP-only OpenIGTLink peer for browser clients.

---

## Configuration: `ClientOptions`

```ts
import { Client, type ClientOptions } from "@openigtlink/core/net";

const opts: ClientOptions = {
  defaultDevice: "browser-dashboard-1",
  connectTimeoutMs: 5000,
  receiveTimeoutMs: 30_000,    // applies to receive()/receiveAny()
                               // calls without an inline timeout
  maxMessageSize: 16 * 1024 * 1024, // 16 MiB cap; 0 = no cap
};

const c = await Client.connect("tracker.lab", 18944, opts);
```

| Field | Default | Meaning |
|---|---|---|
| `defaultDevice` | `"typescript"` | Device-name string written into outgoing headers when `send()` isn't given an explicit device name. |
| `connectTimeoutMs` | `10000` | Bound on the initial connect. `undefined` = no app-level cap (OS default still applies). |
| `receiveTimeoutMs` | `undefined` | Default per-call receive budget. `undefined` = block forever. |
| `maxMessageSize` | `0` (no cap) | Pre-parse DoS defence — reject incoming frames whose `bodySize` exceeds this before reading the body bytes. |

All durations are plain `number` milliseconds — TypeScript has no
idiomatic `timedelta`, and `setTimeout` takes ms anyway.

`WsClient` accepts the same options plus an optional `webSocket`
constructor field (`WsClientOptions`).

`Server` and `WsServer` use `ServerOptions` / `WsServerOptions`,
covered next.

---

## Configuration: `ServerOptions` and `WsServerOptions`

Both server types share the same shape, with WebSocket-only
extensions on `WsServerOptions`:

```ts
import { Server, WsServer } from "@openigtlink/core/net";

const tcp = await Server.listen(18944, {
  host: "0.0.0.0",
  defaultDevice: "tracker-relay",
  maxMessageSize: 16 * 1024 * 1024,
  allow: ["127.0.0.1", "10.0.0.0/8", "::1"],
  maxClients: 32,
});

const ws = await WsServer.listen(18945, {
  host: "0.0.0.0",
  allow: ["10.0.0.0/8"],
  allowOrigins: ["https://dashboard.lab.example"],
  maxClients: 50,
});
```

| Field | Default | Meaning |
|---|---|---|
| `host` | `"0.0.0.0"` | Interface to bind. Use `"127.0.0.1"` to refuse non-loopback peers at the OS level (belt-and-braces with `allow`). |
| `defaultDevice` | `""` | Device-name string written into outgoing headers when `peer.send()` isn't given an explicit device name. |
| `maxMessageSize` | `0` (no cap) | Pre-parse DoS defence — reject incoming frames whose `bodySize` exceeds this before buffering the body bytes. |
| `allow` | `undefined` (all peers) | Peer-IP allowlist. Each entry is a literal address (`"127.0.0.1"`, `"::1"`), a CIDR (`"10.0.0.0/8"`, `"fd00::/8"`), or an inclusive range (`"10.0.0.1-10.0.0.99"`). Non-matching peers are rejected at accept time, before any IGTL byte is read. |
| `maxClients` | `0` (unlimited) | Hard cap on concurrent peers. TCP rejects over-cap connections by closing the socket; WebSocket fails the upgrade with HTTP 503. |
| `allowOrigins` *(WS only)* | `undefined` (all origins) | Allowlist for the browser-supplied `Origin` request header. Exact match per entry; `"*"` allows any origin **including** non-browser clients with no header. Without this set, a page on **any** origin can connect — set it for any `WsServer` reachable from a browser. |

The peer-IP and Origin checks are pre-upgrade for `WsServer`
(via `verifyClient`), so a rejected peer never opens a WebSocket
session; on `Server` they run inside the TCP `connection` listener
before the `Peer` object is constructed. Either way no handler
sees a rejected peer.

Hostnames are not resolved — pass IPs only. If your access list
is DNS-based, resolve at boot and pass the addresses.

---

## Error model

All transport errors derive from `TransportError`:

```ts
import {
  TransportError,
  ConnectionClosedError,
  FramingError,
  TransportTimeoutError,
} from "@openigtlink/core/net";

try {
  const env = await c.receive(Status, { timeoutMs: 2000 });
} catch (err) {
  if (err instanceof TransportTimeoutError) {
    // No Status arrived in 2 s; peer is probably busy or unreachable.
  } else if (err instanceof ConnectionClosedError) {
    // Peer disconnected. The client is no longer usable.
  } else if (err instanceof FramingError) {
    // Wire-level corruption (CRC mismatch, malformed header,
    // body_size > maxMessageSize). The client surfaces this and
    // closes; the connection is no longer usable.
  } else {
    throw err;  // unknown
  }
}
```

`onError(handler)` registers a sink for asynchronous errors
surfaced by the dispatch-loop receive. Errors thrown synchronously
by `receive` / `send` propagate to the caller.

---

## Future work

The TS port deliberately ships a smaller surface than `core-py`:

- **Auto-reconnect, offline buffer, TCP keepalive** — not yet
  implemented. The Python `core-py/NET_GUIDE.md` describes these
  for a future TS implementation to mirror. Application code can
  layer simple reconnect on top of `Client` today by catching
  `ConnectionClosedError` and re-calling `Client.connect`.
- **Idle-disconnect** — not yet implemented. Pair `maxClients`
  with periodic application-level liveness probes for now.

Peer-IP allowlists, Origin allowlists, and `maxClients` are
implemented (see "Configuration: `ServerOptions` and
`WsServerOptions`"). When the remaining items land they'll layer
additively — the entry points (`Client.connect`, `Server.listen`,
`WsClient.connect`, `WsServer.listen`) won't change.
