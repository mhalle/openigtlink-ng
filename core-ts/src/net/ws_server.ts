/**
 * Async OpenIGTLink WebSocket server (Node-only).
 *
 * Mirrors the shape of `core-py` `Server.listen_ws` and the
 * server half of `core-cpp` `Server::listen` (for TCP): a
 * per-peer handler registration + a peer object that handlers
 * can use to reply.
 *
 * Typical use:
 *
 *     import { WsServer } from "@openigtlink/core/net";
 *     import { Status, Transform } from "@openigtlink/core/messages";
 *
 *     const server = await WsServer.listen(18945);
 *     server.on(Transform, async (env, peer) => {
 *       const last3 = env.body.matrix.slice(-3);
 *       await peer.send(new Status({
 *         code: 1,
 *         status_message: `got matrix[-3:]=[${last3.join(", ")}]`,
 *       }));
 *     });
 *     await server.serve();
 *
 * Node-only because the `ws` npm package isn't available in
 * browsers. Lives under `@openigtlink/core/net` (the Node
 * subpath); `@openigtlink/core/net/ws` stays strictly
 * browser-safe.
 *
 * Not a drop-in for in-browser "peer" server — a browser cannot
 * open a WebSocket *listener*. If that becomes a need later, the
 * peer-to-peer-via-relay pattern from the design notes is the
 * shape we'd want.
 */

import type { IncomingMessage, Server as HttpServer } from "node:http";
import { createServer as createHttpServer } from "node:http";

import { WebSocketServer, WebSocket, type RawData } from "ws";

import { extractContent, unpackMessage } from "../codec.js";
import { crc64 } from "../runtime/crc64.js";
import { type MessageCtor } from "../runtime/dispatch.js";
import { CrcMismatchError } from "../runtime/errors.js";
import {
  HEADER_SIZE,
  type Header,
  packHeader,
  unpackHeader,
} from "../runtime/header.js";
import {
  ConnectionClosedError,
  FramingError,
  TransportTimeoutError,
} from "./errors.js";
import { type Envelope } from "./envelope.js";
import type { SendableCtor, SendableMessage } from "./types.js";

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

export interface WsServerOptions {
  /** Host to bind. Default: "0.0.0.0" (all interfaces). */
  host?: string;
  /**
   * Upper bound on bodySize this server will accept in a single
   * frame. 0 = unbounded. Default: 0.
   */
  maxMessageSize?: number;
  /**
   * Default device_name put on outgoing frames (overridable per
   * peer.send call). Matches core-py's `default_device`.
   */
  defaultDevice?: string;
}

interface ResolvedWsServerOptions {
  host: string;
  maxMessageSize: number;
  defaultDevice: string;
}

function resolveOptions(opts?: WsServerOptions): ResolvedWsServerOptions {
  return {
    host: opts?.host ?? "0.0.0.0",
    maxMessageSize: opts?.maxMessageSize ?? 0,
    defaultDevice: opts?.defaultDevice ?? "",
  };
}

// ---------------------------------------------------------------------------
// Handler types
// ---------------------------------------------------------------------------

/**
 * Per-peer handler: receives one message at a time plus the peer
 * object it came from (so the handler can send a reply).
 */
export type PeerHandler<T> = (
  env: Envelope<T>,
  peer: WsPeer,
) => Promise<void> | void;

/** Fallback handler for received messages whose type_id has no registered class. */
export type UnknownPeerHandler = (
  rawHeader: Header,
  rawBody: Uint8Array,
  peer: WsPeer,
) => Promise<void> | void;

/**
 * Accept-time callback, fired once per peer just after the
 * WebSocket upgrade completes and before the dispatch loop starts.
 * Use to send a greeting or to stash peer state.
 */
export type PeerConnectedHandler = (peer: WsPeer) => Promise<void> | void;

// ---------------------------------------------------------------------------
// WsPeer
// ---------------------------------------------------------------------------

/** One accepted WebSocket connection. */
export class WsPeer {
  private closedFlag = false;

  constructor(
    private readonly ws: WebSocket,
    private readonly opts: ResolvedWsServerOptions,
    /** Human-readable address, best-effort. */
    public readonly remoteAddress: string,
  ) {}

  /** True once `.close()` is called or the peer has disconnected. */
  get isClosed(): boolean {
    return this.closedFlag ||
      this.ws.readyState === WebSocket.CLOSING ||
      this.ws.readyState === WebSocket.CLOSED;
  }

  /**
   * Frame and send `message` to this peer.
   *
   * Sets the v1 header (see Client.send for the detailed note on
   * why v1 rather than v2 in this codec's current state) and a
   * binary frame. Returns when the WS library accepts the frame
   * into its outbound queue.
   */
  async send<M extends SendableMessage>(
    message: M,
    opts?: { deviceName?: string; timestamp?: bigint },
  ): Promise<void> {
    if (this.isClosed) {
      throw new ConnectionClosedError(
        "cannot send on a closed WS peer",
      );
    }
    const ctor = message.constructor as unknown as SendableCtor & {
      name?: string;
    };
    const typeId = ctor.TYPE_ID;
    if (typeof typeId !== "string") {
      throw new TypeError(
        `${ctor.name ?? "message"} has no TYPE_ID; not a generated ` +
          `OpenIGTLink message class?`,
      );
    }
    const body = message.pack();
    // v1 framing — same rationale as Client.send.
    const header = packHeader({
      version: 1,
      typeId,
      deviceName: opts?.deviceName ?? this.opts.defaultDevice,
      timestamp: opts?.timestamp ?? 0n,
      body,
    });
    const wire = new Uint8Array(header.length + body.length);
    wire.set(header, 0);
    wire.set(body, header.length);

    await new Promise<void>((resolve, reject) => {
      this.ws.send(wire, { binary: true }, (err?: Error) => {
        if (err) reject(new ConnectionClosedError(err.message));
        else resolve();
      });
    });
  }

  /**
   * Close the connection. Idempotent; safe to call from handlers
   * after a fatal message.
   */
  async close(code = 1000, reason = ""): Promise<void> {
    if (this.closedFlag) return;
    this.closedFlag = true;
    if (
      this.ws.readyState === WebSocket.OPEN ||
      this.ws.readyState === WebSocket.CONNECTING
    ) {
      this.ws.close(code, reason);
    }
    // Wait for the underlying socket to actually finish closing so
    // callers can sequence a subsequent spawn on the same port.
    if (this.ws.readyState !== WebSocket.CLOSED) {
      await new Promise<void>((resolve) => {
        const done = () => resolve();
        this.ws.once("close", done);
        // Guard against never-fires (can happen under test).
        setTimeout(done, 2000).unref?.();
      });
    }
  }
}

// ---------------------------------------------------------------------------
// WsServer
// ---------------------------------------------------------------------------

export class WsServer {
  private readonly opts: ResolvedWsServerOptions;
  private readonly handlers = new Map<string, PeerHandler<unknown>>();
  private unknownHandler?: UnknownPeerHandler;
  private peerConnectedHandler?: PeerConnectedHandler;
  private readonly peers = new Set<WsPeer>();
  private closed = false;
  private serveWaiters: (() => void)[] = [];

  private constructor(
    private readonly http: HttpServer,
    private readonly wss: WebSocketServer,
    opts: WsServerOptions | undefined,
  ) {
    this.opts = resolveOptions(opts);
  }

  /**
   * Bind, start listening, and return a ready server. Handlers
   * attached afterwards apply to any peer that connects.
   *
   * Pass `port = 0` for an OS-assigned random free port; read
   * {@link port} after `listen` resolves to learn which one you got.
   */
  static async listen(
    port: number,
    opts?: WsServerOptions,
  ): Promise<WsServer> {
    const http = createHttpServer();
    const wss = new WebSocketServer({ server: http });
    const server = new WsServer(http, wss, opts);

    wss.on("connection", (ws: WebSocket, req: IncomingMessage) => {
      if (server.closed) {
        try { ws.close(1001, "server shutting down"); } catch {}
        return;
      }
      server.acceptPeer(ws, req);
    });

    await new Promise<void>((resolve, reject) => {
      const onError = (err: Error) => {
        http.off("listening", onListening);
        reject(err);
      };
      const onListening = () => {
        http.off("error", onError);
        resolve();
      };
      http.once("error", onError);
      http.once("listening", onListening);
      http.listen(port, server.opts.host);
    });

    return server;
  }

  /** The OS-assigned (or user-supplied) port this server is listening on. */
  get port(): number {
    const addr = this.http.address();
    if (addr === null || typeof addr === "string") {
      throw new Error("WsServer not listening");
    }
    return addr.port;
  }

  /** Register a handler for messages of type `ctor`. Chainable. */
  on<T>(ctor: MessageCtor<T>, handler: PeerHandler<T>): this {
    this.handlers.set(ctor.TYPE_ID, handler as PeerHandler<unknown>);
    return this;
  }

  /** Fallback handler for messages whose type_id isn't registered. */
  onUnknown(handler: UnknownPeerHandler): this {
    this.unknownHandler = handler;
    return this;
  }

  /** Called once per accepted peer, before its dispatch loop starts. */
  onPeerConnected(handler: PeerConnectedHandler): this {
    this.peerConnectedHandler = handler;
    return this;
  }

  /**
   * Resolve when the server has stopped accepting peers and every
   * in-flight peer has finished. `close()` triggers completion.
   *
   * Shaped to match core-py's `await server.serve()`.
   */
  async serve(): Promise<void> {
    if (this.closed) return;
    return new Promise<void>((resolve) => {
      this.serveWaiters.push(resolve);
    });
  }

  /**
   * Stop accepting new peers, close all existing peers, shut down
   * the HTTP listener. Idempotent.
   */
  async close(): Promise<void> {
    if (this.closed) return;
    this.closed = true;

    const peerCloses = Array.from(this.peers).map((p) =>
      p.close().catch(() => undefined),
    );
    await Promise.all(peerCloses);

    await new Promise<void>((resolve) => {
      this.wss.close(() => resolve());
    });
    await new Promise<void>((resolve) => {
      this.http.close(() => resolve());
    });

    const waiters = this.serveWaiters.splice(0);
    for (const w of waiters) w();
  }

  // ---- internals --------------------------------------------------------

  private acceptPeer(ws: WebSocket, req: IncomingMessage): void {
    const remote = formatRemote(req);
    const peer = new WsPeer(ws, this.opts, remote);
    this.peers.add(peer);

    // `ws` library emits its own close event; remove the peer
    // from the set + fall through for any post-close bookkeeping.
    ws.once("close", () => {
      this.peers.delete(peer);
    });
    ws.on("error", () => {
      // Error events are non-fatal at the server level; the peer
      // will close shortly after. Swallow to prevent the process
      // from crashing on unhandled 'error' on the ws instance.
    });

    // Fire peer-connected callback, then start the dispatch loop.
    const start = async () => {
      if (this.peerConnectedHandler) {
        try {
          await this.peerConnectedHandler(peer);
        } catch (err) {
          // eslint-disable-next-line no-console
          console.error("onPeerConnected handler threw:", err);
        }
      }
      this.startDispatch(peer, ws);
    };
    start();
  }

  private startDispatch(peer: WsPeer, ws: WebSocket): void {
    ws.on("message", async (raw: RawData, isBinary: boolean) => {
      if (!isBinary) {
        // Text frames are never valid OIGTL; drop silently (and
        // document here for future debuggers).
        return;
      }
      const bytes = rawDataToBytes(raw);
      try {
        this.dispatchOne(bytes, peer).catch((err) => {
          // eslint-disable-next-line no-console
          console.error("WsServer dispatch error:", err);
        });
      } catch (err) {
        // eslint-disable-next-line no-console
        console.error("WsServer dispatch error:", err);
      }
    });
  }

  private async dispatchOne(
    bytes: Uint8Array,
    peer: WsPeer,
  ): Promise<void> {
    // Parse the frame: header + body + CRC check + type dispatch.
    if (bytes.length < HEADER_SIZE) {
      throw new FramingError(
        `WS binary frame ${bytes.length} bytes < header ${HEADER_SIZE}`,
      );
    }
    const header = unpackHeader(bytes.subarray(0, HEADER_SIZE));
    const bodySize = Number(header.bodySize);
    if (!Number.isSafeInteger(bodySize)) {
      throw new FramingError(
        `bodySize ${header.bodySize} exceeds Number.MAX_SAFE_INTEGER`,
      );
    }
    if (
      this.opts.maxMessageSize > 0 &&
      bodySize > this.opts.maxMessageSize
    ) {
      throw new FramingError(
        `bodySize ${bodySize} exceeds maxMessageSize ${this.opts.maxMessageSize}`,
      );
    }
    const expected = HEADER_SIZE + bodySize;
    if (bytes.length !== expected) {
      throw new FramingError(
        `WS frame size ${bytes.length} does not match declared ` +
          `bodySize=${bodySize} (expected ${expected})`,
      );
    }
    const body = bytes.subarray(HEADER_SIZE);

    // CRC verification — the WS-server path has no framer, so we
    // check here rather than in unpackMessage.
    const computed = crc64(body);
    if (computed !== header.crc) {
      throw new CrcMismatchError(header.crc, computed);
    }

    const handler = this.handlers.get(header.typeId);
    if (handler !== undefined) {
      // Use loose=true + verifyCrc=false — CRC already checked; any
      // body-decode error surfaces as ProtocolError and is logged
      // by the outer catch.
      const env = unpackMessage(header, body, {
        loose: true,
        verifyCrc: false,
      });
      await handler(env, peer);
      return;
    }

    if (this.unknownHandler !== undefined) {
      // Extract v2/v3 content if applicable so the unknown handler
      // sees the semantically-relevant bytes, not the wrapper.
      const content = extractContent(header, body);
      await this.unknownHandler(header, content, peer);
    }
    // Otherwise: silently drop the message. Matches core-py's
    // default-unknown behavior.
  }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function rawDataToBytes(data: RawData): Uint8Array {
  if (data instanceof ArrayBuffer) {
    return new Uint8Array(data);
  }
  if (ArrayBuffer.isView(data)) {
    const v = data as ArrayBufferView;
    return new Uint8Array(v.buffer, v.byteOffset, v.byteLength);
  }
  if (Array.isArray(data)) {
    // ws may deliver a list of Buffers; concatenate.
    const parts = data.map((d) =>
      ArrayBuffer.isView(d)
        ? new Uint8Array(d.buffer, d.byteOffset, d.byteLength)
        : new Uint8Array(d as ArrayBuffer),
    );
    const total = parts.reduce((n, p) => n + p.length, 0);
    const out = new Uint8Array(total);
    let off = 0;
    for (const p of parts) {
      out.set(p, off);
      off += p.length;
    }
    return out;
  }
  throw new FramingError(
    `unexpected WS frame payload: ${Object.prototype.toString.call(data)}`,
  );
}

function formatRemote(req: IncomingMessage): string {
  const sock = req.socket;
  if (sock && typeof sock.remoteAddress === "string") {
    const port = sock.remotePort ?? "?";
    return `${sock.remoteAddress}:${port}`;
  }
  return "<unknown>";
}
