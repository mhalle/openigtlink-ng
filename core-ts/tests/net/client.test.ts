/**
 * End-to-end tests for the async {@link Client}.
 *
 * Uses Node's `net.createServer` as an in-process loopback so the
 * tests exercise real framing + CRC + socket paths rather than a
 * mock. Same scope as Python's Phase 2 happy-path tests.
 */

import assert from "node:assert/strict";
import { createServer, type Server as TcpServer, type Socket } from "node:net";
import { describe, it } from "node:test";
import { setTimeout as delay } from "node:timers/promises";

import {
  Client,
  ConnectionClosedError,
  RawBody,
  TransportTimeoutError,
} from "../../src/net/index.js";
import "../../src/messages/index.js";
import { Status } from "../../src/messages/status.js";
import { Transform } from "../../src/messages/transform.js";
import { HEADER_SIZE, packHeader, unpackHeader } from "../../src/runtime/header.js";

// ---------------------------------------------------------------------------
// Loopback harness — a tiny test-only server. Accepts one connection,
// runs the provided handler, closes when stopped.
// ---------------------------------------------------------------------------

interface Harness {
  port: number;
  stop(): Promise<void>;
  /** Currently-connected peers, so tests can force-close them. */
  peers: Set<Socket>;
}

async function listenLoopback(
  onPeer: (socket: Socket) => Promise<void> | void,
): Promise<Harness> {
  const peers = new Set<Socket>();
  const server: TcpServer = createServer((socket) => {
    peers.add(socket);
    socket.on("close", () => peers.delete(socket));
    // Run the handler in its own microtask so errors surface on
    // the server side instead of crashing the test runner.
    Promise.resolve().then(() => onPeer(socket)).catch((err) => {
      // eslint-disable-next-line no-console
      console.error("loopback handler failed:", err);
    });
  });

  await new Promise<void>((resolve) => server.listen(0, "127.0.0.1", resolve));
  const addr = server.address();
  if (addr === null || typeof addr === "string") {
    throw new Error("server.address() did not return an AddressInfo");
  }
  return {
    port: addr.port,
    peers,
    stop: () =>
      new Promise<void>((resolve) => {
        for (const p of peers) p.destroy();
        server.close(() => resolve());
      }),
  };
}

/** Read one full IGTL frame from a socket. */
async function readOneFrame(
  socket: Socket,
): Promise<{ typeId: string; body: Uint8Array }> {
  return new Promise((resolve, reject) => {
    const chunks: Buffer[] = [];
    let collected = 0;
    let bodySize = -1;

    const onData = (chunk: Buffer) => {
      chunks.push(chunk);
      collected += chunk.length;
      if (bodySize === -1 && collected >= HEADER_SIZE) {
        const flat = Buffer.concat(chunks);
        const header = unpackHeader(flat);
        bodySize = HEADER_SIZE + Number(header.bodySize);
      }
      if (bodySize !== -1 && collected >= bodySize) {
        socket.off("data", onData);
        socket.off("error", reject);
        const all = Buffer.concat(chunks);
        const header = unpackHeader(all);
        resolve({
          typeId: header.typeId,
          body: new Uint8Array(
            all.buffer,
            all.byteOffset + HEADER_SIZE,
            Number(header.bodySize),
          ),
        });
      }
    };

    socket.on("data", onData);
    socket.once("error", reject);
  });
}

function writeFrame(
  socket: Socket,
  typeId: string,
  body: Uint8Array,
  deviceName = "srv",
): void {
  // version=1: body is bare (no v2 extended-header region).
  const header = packHeader({
    version: 1,
    typeId,
    deviceName,
    timestamp: 0n,
    body,
  });
  socket.write(header);
  socket.write(body);
}

// ---------------------------------------------------------------------------
// connect / close
// ---------------------------------------------------------------------------

describe("Client.connect and close", () => {
  it("round-trips peer info", async () => {
    const srv = await listenLoopback(async (_sock) => {
      /* no-op — just accept */
      await delay(2000);
    });
    const c = await Client.connect("127.0.0.1", srv.port);
    try {
      const peer = c.peer();
      assert.notEqual(peer, null);
      assert.equal(peer?.port, srv.port);
      assert.equal(c.isConnected, true);
    } finally {
      await c.close();
      await srv.stop();
    }
  });

  it("connectTimeoutMs raises TransportTimeoutError", async () => {
    // TEST-NET-2, guaranteed unrouted. If the OS returns ECONNREFUSED
    // (some hosts do), accept either error type.
    await assert.rejects(
      Client.connect("198.51.100.1", 1, { connectTimeoutMs: 100 }),
      (err: unknown) =>
        err instanceof TransportTimeoutError ||
        err instanceof ConnectionClosedError,
    );
  });
});

// ---------------------------------------------------------------------------
// async disposable (TS 5.2+ `await using`)
// ---------------------------------------------------------------------------

describe("Client disposable", () => {
  it("Symbol.asyncDispose closes the connection", async () => {
    const srv = await listenLoopback(async (_sock) => {
      await delay(2000);
    });
    try {
      const c = await Client.connect("127.0.0.1", srv.port);
      await c[Symbol.asyncDispose]();
      await assert.rejects(
        c.send(new Transform()),
        (err: unknown) => err instanceof ConnectionClosedError,
      );
    } finally {
      await srv.stop();
    }
  });
});

// ---------------------------------------------------------------------------
// send / receive
// ---------------------------------------------------------------------------

describe("Client.send / receive", () => {
  it("typed round-trip: Transform → Status", async () => {
    const srv = await listenLoopback(async (sock) => {
      const { typeId } = await readOneFrame(sock);
      assert.equal(typeId, "TRANSFORM");
      const status = new Status({
        code: 1,
        subcode: 0n,
        error_name: "",
        status_message: "ok",
      });
      writeFrame(sock, "STATUS", status.pack(), "echo");
    });
    const c = await Client.connect("127.0.0.1", srv.port);
    try {
      await c.send(
        new Transform({ matrix: [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0] }),
      );
      const env = await c.receive(Status, { timeoutMs: 2000 });
      assert.ok(env.body instanceof Status);
      assert.equal(env.body.code, 1);
      assert.equal(env.body.status_message, "ok");
      assert.equal(env.header.typeId, "STATUS");
      assert.equal(env.header.deviceName, "echo");
    } finally {
      await c.close();
      await srv.stop();
    }
  });

  it("receiveAny yields the next message regardless of type", async () => {
    const srv = await listenLoopback(async (sock) => {
      await readOneFrame(sock);
      writeFrame(
        sock,
        "STATUS",
        new Status({ code: 1, status_message: "any" }).pack(),
      );
    });
    const c = await Client.connect("127.0.0.1", srv.port);
    try {
      await c.send(new Transform());
      const env = await c.receiveAny({ timeoutMs: 2000 });
      assert.equal(env.header.typeId, "STATUS");
      assert.ok(env.body instanceof Status);
    } finally {
      await c.close();
      await srv.stop();
    }
  });

  it("receive() timeout raises TransportTimeoutError", async () => {
    const srv = await listenLoopback(async (_sock) => {
      // Never reply.
      await delay(5000);
    });
    const c = await Client.connect("127.0.0.1", srv.port);
    try {
      await assert.rejects(
        c.receive(Status, { timeoutMs: 100 }),
        (err: unknown) => err instanceof TransportTimeoutError,
      );
    } finally {
      await c.close();
      await srv.stop();
    }
  });

  it("send after peer close raises ConnectionClosedError", async () => {
    const srv = await listenLoopback((sock) => {
      // Immediately drop.
      sock.destroy();
    });
    const c = await Client.connect("127.0.0.1", srv.port);
    try {
      // Give the FIN a moment to arrive.
      await delay(100);
      await assert.rejects(
        (async () => {
          for (let i = 0; i < 5; i++) {
            await c.send(new Transform());
            await delay(20);
          }
        })(),
        (err: unknown) => err instanceof ConnectionClosedError,
      );
    } finally {
      await c.close();
      await srv.stop();
    }
  });
});

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

describe("Client dispatch", () => {
  it("on(T, handler) routes by type_id", async () => {
    const srv = await listenLoopback(async (sock) => {
      await readOneFrame(sock);
      writeFrame(
        sock,
        "TRANSFORM",
        new Transform({ matrix: [1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 2, 3] }).pack(),
      );
      writeFrame(
        sock,
        "STATUS",
        new Status({ code: 1, status_message: "alive" }).pack(),
      );
      writeFrame(
        sock,
        "TRANSFORM",
        new Transform({ matrix: [1, 0, 0, 0, 1, 0, 0, 0, 1, 4, 5, 6] }).pack(),
      );
      await delay(400);
    });
    const c = await Client.connect("127.0.0.1", srv.port);
    try {
      const transforms: number[][] = [];
      const statuses: string[] = [];
      c.on(Transform, (env) => {
        transforms.push(env.body.matrix.slice(-3));
      });
      c.on(Status, (env) => {
        statuses.push(env.body.status_message);
      });
      await c.send(new Transform());
      // Drive dispatch for ~300 ms then close, which ends run().
      const runPromise = c.run();
      await delay(300);
      await c.close();
      await runPromise;

      assert.deepEqual(transforms, [
        [1, 2, 3],
        [4, 5, 6],
      ]);
      assert.deepEqual(statuses, ["alive"]);
    } finally {
      await srv.stop();
    }
  });

  it("receive(T) dispatches intermediate types to handlers", async () => {
    const srv = await listenLoopback(async (sock) => {
      await readOneFrame(sock);
      writeFrame(
        sock,
        "STATUS",
        new Status({ code: 1, status_message: "ignored" }).pack(),
      );
      writeFrame(
        sock,
        "TRANSFORM",
        new Transform({ matrix: [1, 0, 0, 0, 1, 0, 0, 0, 1, 9, 9, 9] }).pack(),
      );
    });
    const c = await Client.connect("127.0.0.1", srv.port);
    try {
      const seenStatuses: string[] = [];
      c.on(Status, (env) => {
        seenStatuses.push(env.body.status_message);
      });

      await c.send(new Transform());
      const env = await c.receive(Transform, { timeoutMs: 2000 });
      assert.deepEqual(env.body.matrix.slice(-3), [9, 9, 9]);
      assert.deepEqual(seenStatuses, ["ignored"]);
    } finally {
      await c.close();
      await srv.stop();
    }
  });

  it("onUnknown handles typed messages with no registered handler", async () => {
    const srv = await listenLoopback(async (sock) => {
      await readOneFrame(sock);
      writeFrame(
        sock,
        "STATUS",
        new Status({ code: 1, status_message: "unhandled" }).pack(),
      );
      await delay(400);
    });
    const c = await Client.connect("127.0.0.1", srv.port);
    try {
      const unknown: string[] = [];
      c.onUnknown((env) => {
        // The body type depends on REGISTRY membership — typed
        // when known, RawBody when not. Either way, header.typeId
        // is the canonical identity.
        unknown.push(env.header.typeId);
      });
      await c.send(new Transform());
      const runPromise = c.run();
      await delay(200);
      await c.close();
      await runPromise;
      assert.deepEqual(unknown, ["STATUS"]);
    } finally {
      await srv.stop();
    }
  });
});
