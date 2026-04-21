/**
 * End-to-end tests for the WebSocket {@link WsClient}.
 *
 * The `ws` npm package provides both the Node-side WebSocket
 * client (plugged into WsClient via `{ webSocket: WebSocket }`)
 * and the test fixture server. Tests run on Node 20+ without
 * requiring the native `globalThis.WebSocket` (which arrived in
 * Node 22 as experimental).
 */

import assert from "node:assert/strict";
import { describe, it } from "node:test";
import { setTimeout as delay } from "node:timers/promises";

import WebSocket, { WebSocketServer } from "ws";

import {
  ConnectionClosedError,
  RawBody,
  TransportTimeoutError,
  WsClient,
  type WebSocketLikeCtor,
} from "../../src/net/ws/index.js";
import "../../src/messages/index.js";
import { Status } from "../../src/messages/status.js";
import { Transform } from "../../src/messages/transform.js";
import { HEADER_SIZE, packHeader, unpackHeader } from "../../src/runtime/header.js";

// ---------------------------------------------------------------------------
// Node-side fixture: a tiny WS echo/reply server using `ws`.
// ---------------------------------------------------------------------------

interface Harness {
  port: number;
  stop(): Promise<void>;
}

async function listenWs(
  onPeer: (ws: WebSocket) => Promise<void> | void,
): Promise<Harness> {
  const wss = new WebSocketServer({ host: "127.0.0.1", port: 0 });
  wss.on("connection", (ws) => {
    Promise.resolve().then(() => onPeer(ws)).catch((err) => {
      // eslint-disable-next-line no-console
      console.error("ws harness handler failed:", err);
    });
  });
  await new Promise<void>((resolve) => wss.once("listening", resolve));
  const addr = wss.address();
  if (addr === null || typeof addr === "string") {
    throw new Error("WebSocketServer.address() did not return AddressInfo");
  }
  return {
    port: addr.port,
    stop: () =>
      new Promise<void>((resolve) => {
        for (const client of wss.clients) {
          try { client.terminate(); } catch { /* noop */ }
        }
        wss.close(() => resolve());
      }),
  };
}

/** Read the first full IGTL frame from a `ws` peer. */
function readOneFrame(ws: WebSocket): Promise<{
  typeId: string;
  body: Uint8Array;
}> {
  return new Promise((resolve, reject) => {
    ws.once("message", (data, isBinary) => {
      if (!isBinary) {
        reject(new Error("received non-binary WS frame"));
        return;
      }
      const buf = data as Buffer;
      const bytes = new Uint8Array(buf.buffer, buf.byteOffset, buf.byteLength);
      const header = unpackHeader(bytes.subarray(0, HEADER_SIZE));
      resolve({
        typeId: header.typeId,
        body: bytes.slice(HEADER_SIZE, HEADER_SIZE + Number(header.bodySize)),
      });
    });
    ws.once("error", reject);
  });
}

function writeFrame(
  ws: WebSocket,
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
  const wire = new Uint8Array(header.length + body.length);
  wire.set(header, 0);
  wire.set(body, header.length);
  ws.send(wire);
}

// The `ws` package's WebSocket class is compatible with our
// WebSocketLikeCtor shape. Cast once and reuse.
const WsCtor = WebSocket as unknown as WebSocketLikeCtor;

// ---------------------------------------------------------------------------
// Connect / close
// ---------------------------------------------------------------------------

describe("WsClient.connect and close", () => {
  it("reports url and isConnected", async () => {
    const srv = await listenWs(async (_ws) => {
      await delay(2000);
    });
    try {
      const url = `ws://127.0.0.1:${srv.port}/`;
      const c = await WsClient.connect(url, { webSocket: WsCtor });
      try {
        assert.equal(c.url, url);
        assert.equal(c.isConnected, true);
      } finally {
        await c.close();
      }
    } finally {
      await srv.stop();
    }
  });

  it("connectTimeoutMs raises TransportTimeoutError", async () => {
    // 198.51.100.0/24 TEST-NET-2 — guaranteed unrouted. Different
    // OSes may also produce ConnectionClosedError.
    await assert.rejects(
      WsClient.connect("ws://198.51.100.1:1/", {
        webSocket: WsCtor,
        connectTimeoutMs: 100,
      }),
      (err: unknown) =>
        err instanceof TransportTimeoutError ||
        err instanceof ConnectionClosedError,
    );
  });

  it("rejects non-ws URL scheme", async () => {
    await assert.rejects(
      WsClient.connect("tcp://127.0.0.1:18944", { webSocket: WsCtor }),
    );
  });
});

// ---------------------------------------------------------------------------
// Async disposable
// ---------------------------------------------------------------------------

describe("WsClient disposable", () => {
  it("Symbol.asyncDispose closes the connection", async () => {
    const srv = await listenWs(async (_ws) => { await delay(2000); });
    try {
      const url = `ws://127.0.0.1:${srv.port}/`;
      const c = await WsClient.connect(url, { webSocket: WsCtor });
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
// Send / receive
// ---------------------------------------------------------------------------

describe("WsClient.send / receive", () => {
  it("typed round-trip: Transform → Status", async () => {
    const srv = await listenWs(async (ws) => {
      const { typeId } = await readOneFrame(ws);
      assert.equal(typeId, "TRANSFORM");
      writeFrame(
        ws,
        "STATUS",
        new Status({
          code: 1,
          subcode: 0n,
          error_name: "",
          status_message: "ws-ok",
        }).pack(),
        "echo",
      );
    });
    try {
      const c = await WsClient.connect(
        `ws://127.0.0.1:${srv.port}/`,
        { webSocket: WsCtor },
      );
      try {
        await c.send(
          new Transform({ matrix: [1,0,0, 0,1,0, 0,0,1, 0,0,0] }),
        );
        const env = await c.receive(Status, { timeoutMs: 2000 });
        assert.ok(env.body instanceof Status);
        assert.equal(env.body.code, 1);
        assert.equal(env.body.status_message, "ws-ok");
        assert.equal(env.header.typeId, "STATUS");
        assert.equal(env.header.deviceName, "echo");
      } finally {
        await c.close();
      }
    } finally {
      await srv.stop();
    }
  });

  it("receiveAny yields the next message regardless of type", async () => {
    const srv = await listenWs(async (ws) => {
      await readOneFrame(ws);
      writeFrame(
        ws,
        "STATUS",
        new Status({ code: 1, status_message: "any-ws" }).pack(),
      );
    });
    try {
      const c = await WsClient.connect(
        `ws://127.0.0.1:${srv.port}/`,
        { webSocket: WsCtor },
      );
      try {
        await c.send(new Transform());
        const env = await c.receiveAny({ timeoutMs: 2000 });
        assert.equal(env.header.typeId, "STATUS");
        assert.ok(env.body instanceof Status);
      } finally {
        await c.close();
      }
    } finally {
      await srv.stop();
    }
  });

  it("receive() timeout raises TransportTimeoutError", async () => {
    const srv = await listenWs(async (_ws) => { await delay(5000); });
    try {
      const c = await WsClient.connect(
        `ws://127.0.0.1:${srv.port}/`,
        { webSocket: WsCtor },
      );
      try {
        await assert.rejects(
          c.receive(Status, { timeoutMs: 100 }),
          (err: unknown) => err instanceof TransportTimeoutError,
        );
      } finally {
        await c.close();
      }
    } finally {
      await srv.stop();
    }
  });

  it("send after peer close raises ConnectionClosedError", async () => {
    const srv = await listenWs((ws) => {
      // Close the WS right away.
      ws.terminate();
    });
    try {
      const c = await WsClient.connect(
        `ws://127.0.0.1:${srv.port}/`,
        { webSocket: WsCtor },
      );
      try {
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
      }
    } finally {
      await srv.stop();
    }
  });
});

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

describe("WsClient dispatch", () => {
  it("on(T, handler) routes by type_id", async () => {
    const srv = await listenWs(async (ws) => {
      await readOneFrame(ws);
      writeFrame(
        ws,
        "TRANSFORM",
        new Transform({ matrix: [1,0,0, 0,1,0, 0,0,1, 1,2,3] }).pack(),
      );
      writeFrame(
        ws,
        "STATUS",
        new Status({ code: 1, status_message: "alive" }).pack(),
      );
      writeFrame(
        ws,
        "TRANSFORM",
        new Transform({ matrix: [1,0,0, 0,1,0, 0,0,1, 4,5,6] }).pack(),
      );
      await delay(400);
    });
    try {
      const c = await WsClient.connect(
        `ws://127.0.0.1:${srv.port}/`,
        { webSocket: WsCtor },
      );
      try {
        const transforms: number[][] = [];
        const statuses: string[] = [];
        c.on(Transform, (env) => { transforms.push(env.body.matrix.slice(-3)); });
        c.on(Status, (env) => { statuses.push(env.body.status_message); });

        await c.send(new Transform());
        const runPromise = c.run();
        await delay(300);
        await c.close();
        await runPromise;

        assert.deepEqual(transforms, [[1, 2, 3], [4, 5, 6]]);
        assert.deepEqual(statuses, ["alive"]);
      } finally {
        await c.close();
      }
    } finally {
      await srv.stop();
    }
  });

  it("receive(T) dispatches intermediate types to handlers", async () => {
    const srv = await listenWs(async (ws) => {
      await readOneFrame(ws);
      writeFrame(
        ws,
        "STATUS",
        new Status({ code: 1, status_message: "ignored" }).pack(),
      );
      writeFrame(
        ws,
        "TRANSFORM",
        new Transform({ matrix: [1,0,0, 0,1,0, 0,0,1, 9,9,9] }).pack(),
      );
    });
    try {
      const c = await WsClient.connect(
        `ws://127.0.0.1:${srv.port}/`,
        { webSocket: WsCtor },
      );
      try {
        const seenStatuses: string[] = [];
        c.on(Status, (env) => { seenStatuses.push(env.body.status_message); });

        await c.send(new Transform());
        const env = await c.receive(Transform, { timeoutMs: 2000 });
        assert.deepEqual(env.body.matrix.slice(-3), [9, 9, 9]);
        assert.deepEqual(seenStatuses, ["ignored"]);
      } finally {
        await c.close();
      }
    } finally {
      await srv.stop();
    }
  });

  it("onUnknown fires for typeIds with no registered handler", async () => {
    const srv = await listenWs(async (ws) => {
      await readOneFrame(ws);
      writeFrame(
        ws,
        "STATUS",
        new Status({ code: 1, status_message: "unhandled" }).pack(),
      );
      await delay(400);
    });
    try {
      const c = await WsClient.connect(
        `ws://127.0.0.1:${srv.port}/`,
        { webSocket: WsCtor },
      );
      try {
        const unknown: string[] = [];
        c.onUnknown((env) => {
          unknown.push(env.header.typeId);
          // When the type IS registered (STATUS is), env.body is
          // a decoded Status instance; when not, it's a RawBody.
          // Here STATUS is in the REGISTRY so the assertion is
          // the inverse — body is NOT a RawBody sentinel.
          assert.ok(!(env.body instanceof RawBody));
        });
        await c.send(new Transform());
        const runPromise = c.run();
        await delay(200);
        await c.close();
        await runPromise;
        assert.deepEqual(unknown, ["STATUS"]);
      } finally {
        await c.close();
      }
    } finally {
      await srv.stop();
    }
  });
});
