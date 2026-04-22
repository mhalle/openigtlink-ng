/**
 * WsServer unit + loopback tests.
 *
 * Drives:
 *   - listen/close lifecycle (random port via 0, port reporting,
 *     idempotent close)
 *   - handler dispatch: on(T, handler) routes by type_id, peer.send
 *     works as a reply inside handlers
 *   - onPeerConnected: fires once per accepted peer, before any
 *     dispatch
 *   - onUnknown: fires for typeIds with no registered handler
 *   - End-to-end ts↔ts loopback: a real WsClient drives a real
 *     WsServer over a real OS socket, byte-for-byte through the
 *     codec. Closes the last self-tested gap in the interop matrix
 *     for TS-only deployments.
 *
 * The separate `cross_runtime_ts_ws.*` tests (core-py → ts-ws)
 * pick up where this one stops — proving the same server byte-
 * compatibly serves a foreign-language client.
 */

import assert from "node:assert/strict";
import WebSocket from "ws";

import { describe, it } from "node:test";

import "../../src/messages/index.js";   // side effect: register types
import { Status } from "../../src/messages/status.js";
import { Transform } from "../../src/messages/transform.js";
import { WsClient, type WebSocketLikeCtor } from "../../src/net/ws_client.js";
import { WsServer, WsPeer } from "../../src/net/ws_server.js";

const WsCtor = WebSocket as unknown as WebSocketLikeCtor;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

describe("WsServer lifecycle", () => {
  it("binds on port 0 and reports the assigned port", async () => {
    const server = await WsServer.listen(0, { host: "127.0.0.1" });
    try {
      assert.ok(server.port > 0 && server.port < 65536);
    } finally {
      await server.close();
    }
  });

  it("close() is idempotent", async () => {
    const server = await WsServer.listen(0, { host: "127.0.0.1" });
    await server.close();
    await server.close();   // no-op, no throw
  });

  it("serve() resolves when close() is called", async () => {
    const server = await WsServer.listen(0, { host: "127.0.0.1" });
    const servePromise = server.serve();
    // Close shortly after; serve() should resolve.
    setTimeout(() => { server.close(); }, 50);
    await servePromise;
  });
});

// ---------------------------------------------------------------------------
// Dispatch: ts-ts loopback
// ---------------------------------------------------------------------------

describe("WsServer dispatch — ts WsClient ↔ ts WsServer loopback", () => {
  it("routes TRANSFORM to its registered handler; peer.send replies work", async () => {
    const server = await WsServer.listen(0, { host: "127.0.0.1" });
    const state: { peer: WsPeer | null } = { peer: null };
    server.on(Transform, async (env, peer) => {
      state.peer = peer;
      const last3 = env.body.matrix.slice(-3);
      await peer.send(new Status({
        code: 1,
        subcode: 0n,
        error_name: "",
        status_message: `got matrix[-3:]=[${last3.join(", ")}]`,
      }));
    });

    try {
      const c = await WsClient.connect(
        `ws://127.0.0.1:${server.port}/`,
        { webSocket: WsCtor, connectTimeoutMs: 5000 },
      );
      try {
        await c.send(new Transform({
          matrix: [1, 0, 0, 0, 1, 0, 0, 0, 1, 11, 22, 33],
        }));
        const reply = await c.receive(Status, { timeoutMs: 5000 });
        assert.equal(reply.body.code, 1);
        assert.match(
          reply.body.status_message,
          /matrix\[-3:\]=\[11, 22, 33\]/,
        );
        assert.ok(state.peer instanceof WsPeer);
      } finally {
        await c.close();
      }
    } finally {
      await server.close();
    }
  });

  it("onPeerConnected fires once per peer, before dispatch", async () => {
    const server = await WsServer.listen(0, { host: "127.0.0.1" });
    const peersSeen: WsPeer[] = [];
    server.onPeerConnected((peer) => { peersSeen.push(peer); });

    try {
      const c1 = await WsClient.connect(
        `ws://127.0.0.1:${server.port}/`,
        { webSocket: WsCtor, connectTimeoutMs: 5000 },
      );
      const c2 = await WsClient.connect(
        `ws://127.0.0.1:${server.port}/`,
        { webSocket: WsCtor, connectTimeoutMs: 5000 },
      );
      // Give the server a tick to process the second connection.
      await new Promise((r) => setTimeout(r, 100));
      assert.equal(peersSeen.length, 2);
      await c1.close();
      await c2.close();
    } finally {
      await server.close();
    }
  });

  it("onUnknown fires for typeIds with no registered handler", async () => {
    const server = await WsServer.listen(0, { host: "127.0.0.1" });
    let unknownFired = false;
    server.onUnknown(async (header, _content, _peer) => {
      if (header.typeId === "TRANSFORM") unknownFired = true;
    });

    try {
      const c = await WsClient.connect(
        `ws://127.0.0.1:${server.port}/`,
        { webSocket: WsCtor, connectTimeoutMs: 5000 },
      );
      try {
        await c.send(new Transform({
          matrix: [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0],
        }));
        // Give the server a tick to dispatch.
        await new Promise((r) => setTimeout(r, 100));
        assert.equal(unknownFired, true);
      } finally {
        await c.close();
      }
    } finally {
      await server.close();
    }
  });
});
