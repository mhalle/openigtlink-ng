/**
 * TCP Server unit + loopback tests.
 *
 * Covers:
 *   - listen/port/close lifecycle (random-port via 0, idempotent
 *     close, serve() resolves on close)
 *   - handler dispatch: on(T, handler) routes by type_id, peer.send
 *     works inside a handler
 *   - onPeerConnected fires once per accepted peer before dispatch
 *   - onUnknown fallback
 *   - End-to-end ts Client ↔ ts Server loopback over a real socket
 *     — closes the last intra-TS gap (TCP) after the existing
 *     WsClient ↔ WsServer loopback covered the WS side.
 */

import assert from "node:assert/strict";
import { describe, it } from "node:test";

import "../../src/messages/index.js";   // side effect: register types
import { Status } from "../../src/messages/status.js";
import { Transform } from "../../src/messages/transform.js";
import { Client } from "../../src/net/client.js";
import { Peer, Server } from "../../src/net/server.js";

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

describe("Server lifecycle", () => {
  it("binds on port 0 and reports the assigned port", async () => {
    const server = await Server.listen(0, { host: "127.0.0.1" });
    try {
      assert.ok(server.port > 0 && server.port < 65536);
    } finally {
      await server.close();
    }
  });

  it("close() is idempotent", async () => {
    const server = await Server.listen(0, { host: "127.0.0.1" });
    await server.close();
    await server.close();   // no-op, no throw
  });

  it("serve() resolves when close() is called", async () => {
    const server = await Server.listen(0, { host: "127.0.0.1" });
    const servePromise = server.serve();
    setTimeout(() => { server.close(); }, 50);
    await servePromise;
  });
});

// ---------------------------------------------------------------------------
// Dispatch: ts-ts loopback
// ---------------------------------------------------------------------------

describe("Server dispatch — ts Client ↔ ts Server loopback", () => {
  it("routes TRANSFORM to its registered handler; peer.send replies", async () => {
    const server = await Server.listen(0, { host: "127.0.0.1" });
    const seen: { peer: Peer | null } = { peer: null };
    server.on(Transform, async (env, peer) => {
      seen.peer = peer;
      const last3 = env.body.matrix.slice(-3);
      await peer.send(new Status({
        code: 1,
        subcode: 0n,
        error_name: "",
        status_message: `got matrix[-3:]=[${last3.join(", ")}]`,
      }));
    });

    try {
      const c = await Client.connect("127.0.0.1", server.port, {
        connectTimeoutMs: 5000,
      });
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
        assert.ok(seen.peer instanceof Peer);
      } finally {
        await c.close();
      }
    } finally {
      await server.close();
    }
  });

  it("onPeerConnected fires once per peer, before dispatch", async () => {
    const server = await Server.listen(0, { host: "127.0.0.1" });
    const peersSeen: Peer[] = [];
    server.onPeerConnected((peer) => { peersSeen.push(peer); });

    try {
      const c1 = await Client.connect("127.0.0.1", server.port, {
        connectTimeoutMs: 5000,
      });
      const c2 = await Client.connect("127.0.0.1", server.port, {
        connectTimeoutMs: 5000,
      });
      // Give the server a tick to process both accepts.
      await new Promise((r) => setTimeout(r, 100));
      assert.equal(peersSeen.length, 2);
      await c1.close();
      await c2.close();
    } finally {
      await server.close();
    }
  });

  it("onUnknown fires for typeIds with no registered handler", async () => {
    const server = await Server.listen(0, { host: "127.0.0.1" });
    let unknownFired = false;
    server.onUnknown(async (header, _content, _peer) => {
      if (header.typeId === "TRANSFORM") unknownFired = true;
    });

    try {
      const c = await Client.connect("127.0.0.1", server.port, {
        connectTimeoutMs: 5000,
      });
      try {
        await c.send(new Transform({
          matrix: [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0],
        }));
        // Give the server a tick to drain the framer and dispatch.
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
