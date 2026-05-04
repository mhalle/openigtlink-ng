/**
 * Async OpenIGTLink TCP server (Node-only).
 *
 * Mirrors the shape of `core-py` `Server.listen` and the server
 * half of `core-cpp` `Server::listen`: per-type handler
 * registration + a peer object that handlers can use to reply.
 * The WebSocket equivalent lives in {@link WsServer}; the two
 * share the same surface so a call site can switch transports
 * by changing one import.
 *
 * Typical use:
 *
 *     import { Server } from "@openigtlink/core/net";
 *     import { Status, Transform } from "@openigtlink/core/messages";
 *
 *     const server = await Server.listen(18944);
 *     server.on(Transform, async (env, peer) => {
 *       const last3 = env.body.matrix.slice(-3);
 *       await peer.send(new Status({
 *         code: 1,
 *         status_message: `got matrix[-3:]=[${last3.join(", ")}]`,
 *       }));
 *     });
 *     await server.serve();
 *
 * Node-only; not browser-importable (Node lacks `net.Server` in
 * the browser). Lives under `@openigtlink/core/net`.
 */

import {
  Server as NetServer,
  Socket,
  createServer as createTcpServer,
} from "node:net";

import { extractContent, unpackMessage } from "../codec.js";
import { type MessageCtor } from "../runtime/dispatch.js";
import {
  HEADER_SIZE,
  type Header,
  packHeader,
} from "../runtime/header.js";
import { ByteAccumulator, V3Framer, type Incoming } from "./framer.js";
import {
  ConnectionClosedError,
  FramingError,
} from "./errors.js";
import { type Envelope } from "./envelope.js";
import { buildPeerPolicy, type PeerPolicy } from "./policy.js";
import type { SendableCtor, SendableMessage } from "./types.js";

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

export interface ServerOptions {
  /** Host to bind. Default: "0.0.0.0" (all interfaces). */
  host?: string;
  /**
   * Upper bound on bodySize the framer will accept from any peer.
   * 0 = unbounded. Enforced at frame-parse time, so a hostile
   * peer announcing a huge bodySize is rejected before buffering.
   * Default: 0.
   */
  maxMessageSize?: number;
  /**
   * Default device_name put on outgoing frames (overridable per
   * `peer.send` call). Matches core-py `default_device`.
   */
  defaultDevice?: string;
  /**
   * Peer-IP allowlist. Each entry is a literal address
   * (`"127.0.0.1"`, `"::1"`), a CIDR (`"10.0.0.0/8"`,
   * `"fd00::/8"`), or an inclusive range
   * (`"10.0.0.1-10.0.0.99"`). Connections from outside the list
   * are rejected at accept time, before the first byte is read.
   *
   * Default: undefined (accept any peer). Mirrors core-py
   * `Server.allow(...)`.
   */
  allow?: readonly string[];
  /**
   * Hard cap on concurrent peers. Once reached, further accepts
   * are closed immediately. `0` = unlimited. Default: 0.
   *
   * Pre-parse DoS hygiene; pair with `allow` for layered defence.
   */
  maxClients?: number;
}

interface ResolvedServerOptions {
  host: string;
  maxMessageSize: number;
  defaultDevice: string;
  policy: PeerPolicy;
  maxClients: number;
}

function resolveOptions(opts?: ServerOptions): ResolvedServerOptions {
  const maxClients = opts?.maxClients ?? 0;
  if (maxClients < 0 || !Number.isInteger(maxClients)) {
    throw new TypeError("maxClients must be a non-negative integer");
  }
  return {
    host: opts?.host ?? "0.0.0.0",
    maxMessageSize: opts?.maxMessageSize ?? 0,
    defaultDevice: opts?.defaultDevice ?? "",
    policy: buildPeerPolicy(opts?.allow),
    maxClients,
  };
}

// ---------------------------------------------------------------------------
// Handler types
// ---------------------------------------------------------------------------

/**
 * Per-peer handler: receives one message at a time plus the peer
 * object it came from (so the handler can send a reply).
 */
export type TcpPeerHandler<T> = (
  env: Envelope<T>,
  peer: Peer,
) => Promise<void> | void;

/** Fallback handler for received messages whose type_id has no registered class. */
export type TcpUnknownPeerHandler = (
  rawHeader: Header,
  rawContent: Uint8Array,
  peer: Peer,
) => Promise<void> | void;

/**
 * Accept-time callback, fired once per peer just after TCP
 * accept and before its dispatch loop starts.
 */
export type TcpPeerConnectedHandler = (
  peer: Peer,
) => Promise<void> | void;

// ---------------------------------------------------------------------------
// Peer
// ---------------------------------------------------------------------------

/** One accepted TCP connection. */
export class Peer {
  private closedFlag = false;
  private readonly framer = new V3Framer();
  private readonly acc = new ByteAccumulator();

  constructor(
    private readonly socket: Socket,
    private readonly opts: ResolvedServerOptions,
    /** Human-readable remote address. */
    public readonly remoteAddress: string,
  ) {}

  /** True once `.close()` is called or the underlying socket has ended. */
  get isClosed(): boolean {
    return this.closedFlag || this.socket.destroyed;
  }

  /**
   * Frame and send `message` to this peer.
   *
   * Sets a v1 header (see Client.send for the rationale) and
   * writes a single framed wire message.
   */
  async send<M extends SendableMessage>(
    message: M,
    opts?: { deviceName?: string; timestamp?: bigint },
  ): Promise<void> {
    if (this.isClosed) {
      throw new ConnectionClosedError("cannot send on a closed Peer");
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
      this.socket.write(wire, (err?: Error | null) => {
        if (err) reject(new ConnectionClosedError(err.message));
        else resolve();
      });
    });
  }

  /**
   * Close the connection. Idempotent; safe to call from handlers
   * after a fatal message.
   */
  async close(): Promise<void> {
    if (this.closedFlag) return;
    this.closedFlag = true;
    if (!this.socket.destroyed) {
      this.socket.end();
    }
    if (this.socket.destroyed) return;
    // Wait for the underlying socket to finish closing so callers
    // can sequence a subsequent spawn on the same port.
    await new Promise<void>((resolve) => {
      const done = () => resolve();
      this.socket.once("close", done);
      setTimeout(done, 2000).unref?.();
    });
  }

  // ---- internals --------------------------------------------------------

  /** Push `chunk` through the framer; emit parsed Incoming values. */
  ingest(
    chunk: Buffer,
    onIncoming: (inc: Incoming) => void,
    onError: (err: unknown) => void,
  ): void {
    try {
      this.acc.push(
        new Uint8Array(chunk.buffer, chunk.byteOffset, chunk.byteLength),
      );
      while (true) {
        const inc = this.framer.tryParse(this.acc);
        if (inc === null) return;
        if (
          this.opts.maxMessageSize > 0 &&
          inc.header.bodySize > BigInt(this.opts.maxMessageSize)
        ) {
          throw new FramingError(
            `bodySize ${inc.header.bodySize} exceeds maxMessageSize ` +
              `${this.opts.maxMessageSize}`,
          );
        }
        onIncoming(inc);
      }
    } catch (err) {
      onError(err);
    }
  }
}

// ---------------------------------------------------------------------------
// Server
// ---------------------------------------------------------------------------

export class Server {
  private readonly opts: ResolvedServerOptions;
  private readonly handlers = new Map<string, TcpPeerHandler<unknown>>();
  private unknownHandler?: TcpUnknownPeerHandler;
  private peerConnectedHandler?: TcpPeerConnectedHandler;
  private readonly peers = new Set<Peer>();
  private closed = false;
  private serveWaiters: (() => void)[] = [];

  private constructor(
    private readonly net: NetServer,
    opts: ServerOptions | undefined,
  ) {
    this.opts = resolveOptions(opts);
  }

  /**
   * Bind, start listening, and return a ready server.
   *
   * Pass `port = 0` for an OS-assigned random free port; read
   * {@link port} after `listen` resolves.
   */
  static async listen(
    port: number,
    opts?: ServerOptions,
  ): Promise<Server> {
    const net = createTcpServer();
    const server = new Server(net, opts);

    net.on("connection", (socket: Socket) => {
      if (server.closed) {
        try { socket.destroy(); } catch {}
        return;
      }
      // Pre-accept policy gates: peer-IP allowlist and max-clients.
      // Both close the socket without ever calling acceptPeer, so
      // no Peer object is constructed and no IGTL byte is read.
      const remoteIp = socket.remoteAddress ?? "";
      if (!server.opts.policy.allows(remoteIp)) {
        try { socket.destroy(); } catch {}
        return;
      }
      if (
        server.opts.maxClients > 0 &&
        server.peers.size >= server.opts.maxClients
      ) {
        try { socket.destroy(); } catch {}
        return;
      }
      server.acceptPeer(socket);
    });

    await new Promise<void>((resolve, reject) => {
      const onError = (err: Error) => {
        net.off("listening", onListening);
        reject(err);
      };
      const onListening = () => {
        net.off("error", onError);
        resolve();
      };
      net.once("error", onError);
      net.once("listening", onListening);
      net.listen(port, server.opts.host);
    });

    return server;
  }

  /** The OS-assigned (or user-supplied) port this server is listening on. */
  get port(): number {
    const addr = this.net.address();
    if (addr === null || typeof addr === "string") {
      throw new Error("Server not listening");
    }
    return addr.port;
  }

  /** Register a handler for messages of type `ctor`. Chainable. */
  on<T>(ctor: MessageCtor<T>, handler: TcpPeerHandler<T>): this {
    this.handlers.set(ctor.TYPE_ID, handler as TcpPeerHandler<unknown>);
    return this;
  }

  /** Fallback handler for messages whose type_id isn't registered. */
  onUnknown(handler: TcpUnknownPeerHandler): this {
    this.unknownHandler = handler;
    return this;
  }

  /** Called once per accepted peer, before its dispatch loop starts. */
  onPeerConnected(handler: TcpPeerConnectedHandler): this {
    this.peerConnectedHandler = handler;
    return this;
  }

  /**
   * Resolve when the server has stopped accepting peers and every
   * in-flight peer has finished. `close()` triggers completion.
   */
  async serve(): Promise<void> {
    if (this.closed) return;
    return new Promise<void>((resolve) => {
      this.serveWaiters.push(resolve);
    });
  }

  /**
   * Stop accepting new peers, close all existing peers, shut down
   * the listener. Idempotent.
   */
  async close(): Promise<void> {
    if (this.closed) return;
    this.closed = true;

    const peerCloses = Array.from(this.peers).map((p) =>
      p.close().catch(() => undefined),
    );
    await Promise.all(peerCloses);

    await new Promise<void>((resolve) => {
      this.net.close(() => resolve());
    });

    const waiters = this.serveWaiters.splice(0);
    for (const w of waiters) w();
  }

  // ---- internals --------------------------------------------------------

  private acceptPeer(socket: Socket): void {
    const remote = formatRemote(socket);
    const peer = new Peer(socket, this.opts, remote);
    this.peers.add(peer);

    socket.once("close", () => {
      this.peers.delete(peer);
    });
    socket.on("error", () => {
      // Per-peer socket errors are non-fatal at the server level;
      // swallow to prevent the process from crashing on unhandled
      // 'error' from a dropped connection.
    });

    const start = async () => {
      if (this.peerConnectedHandler) {
        try {
          await this.peerConnectedHandler(peer);
        } catch (err) {
          // eslint-disable-next-line no-console
          console.error("onPeerConnected handler threw:", err);
        }
      }
      this.startDispatch(peer, socket);
    };
    start();
  }

  private startDispatch(peer: Peer, socket: Socket): void {
    socket.on("data", (chunk: Buffer) => {
      peer.ingest(
        chunk,
        (inc) => {
          this.dispatchOne(inc, peer).catch((err) => {
            // eslint-disable-next-line no-console
            console.error("Server dispatch error:", err);
          });
        },
        (err) => {
          // Framer-level error — the stream is corrupt; close this
          // peer and move on. Other peers are unaffected.
          // eslint-disable-next-line no-console
          console.error("peer framer error:", err);
          peer.close().catch(() => undefined);
        },
      );
    });
  }

  private async dispatchOne(
    inc: Incoming,
    peer: Peer,
  ): Promise<void> {
    const handler = this.handlers.get(inc.header.typeId);
    if (handler !== undefined) {
      // CRC already verified by the framer — skip the second pass.
      const env = unpackMessage(inc.header, inc.body, {
        loose: true,
        verifyCrc: false,
      });
      await handler(env, peer);
      return;
    }

    if (this.unknownHandler !== undefined) {
      const content = extractContent(inc.header, inc.body);
      await this.unknownHandler(inc.header, content, peer);
    }
    // Otherwise silently drop — matches core-py's default.
  }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function formatRemote(socket: Socket): string {
  const addr = socket.remoteAddress ?? "<unknown>";
  const port = socket.remotePort ?? 0;
  return `${addr}:${port}`;
}

// Keep HEADER_SIZE on the public API surface too, matching WsServer.
export { HEADER_SIZE };
