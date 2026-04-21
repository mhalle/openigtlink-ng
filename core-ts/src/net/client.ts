/**
 * Async OpenIGTLink client (Node TCP).
 *
 * Mirrors the *capability* of `oigtl::Client` and `oigtl.net.Client`
 * for the simplest case: connect, send, receive, dispatch. No
 * auto-reconnect, no offline buffer, no keepalive — those layer
 * on later.
 *
 * Researcher-first API:
 *
 *     import { Client } from "@openigtlink/core/net";
 *     import { Transform, Status } from "@openigtlink/core/messages";
 *
 *     const c = await Client.connect("tracker.lab", 18944);
 *     await c.send(new Transform({ matrix: [...] }));
 *     const reply = await c.receive(Status);
 *     await c.close();
 *
 * Or with the dispatch loop:
 *
 *     c.on(Transform, async (env) => renderer.updatePose(env.body.matrix));
 *     c.on(Status,    async (env) => { if (env.body.code !== 1) log(env.body); });
 *     await c.run();
 */

import { Socket, createConnection } from "node:net";
import { setTimeout as delay } from "node:timers/promises";

import { unpackMessage } from "../codec.js";
import { type MessageCtor } from "../runtime/dispatch.js";
import { packHeader } from "../runtime/header.js";
import {
  ConnectionClosedError,
  FramingError,
  TransportError,
  TransportTimeoutError,
} from "./errors.js";
import { ByteAccumulator, V3Framer, type Incoming } from "./framer.js";
import {
  RawBody,
  type Envelope,
} from "./envelope.js";
import {
  resolveClientOptions,
  type ClientOptions,
  type ResolvedClientOptions,
} from "./options.js";

// ---------------------------------------------------------------------------
// Contracts for a sendable typed message — re-exported from
// ./types.ts so both TCP Client and WS WsClient see the same
// types without cross-importing between the two transport
// modules.
// ---------------------------------------------------------------------------

export type {
  Handler,
  SendableCtor,
  SendableMessage,
} from "./types.js";

import type {
  Handler,
  SendableCtor,
  SendableMessage,
} from "./types.js";

// ---------------------------------------------------------------------------
// Internal: a tiny deferred primitive so the socket's `data` event
// can hand values off to whoever's currently in `receive()`.
// ---------------------------------------------------------------------------

interface Deferred<T> {
  readonly promise: Promise<T>;
  resolve(value: T): void;
  reject(err: unknown): void;
}

function deferred<T>(): Deferred<T> {
  let resolve!: (v: T) => void;
  let reject!: (e: unknown) => void;
  const promise = new Promise<T>((res, rej) => {
    resolve = res;
    reject = rej;
  });
  return { promise, resolve, reject };
}

// ---------------------------------------------------------------------------
// Client
// ---------------------------------------------------------------------------

export class Client {
  // Socket, framing, and byte accumulator.
  private readonly socket: Socket;
  private readonly framer = new V3Framer();
  private readonly acc = new ByteAccumulator();
  private readonly opts: ResolvedClientOptions;

  // FIFO of messages read off the wire but not yet consumed. The
  // socket 'data' handler pushes to the tail; `receive()` /
  // `receiveAny()` pop from the head.
  private readonly ready: Incoming[] = [];

  // One waiter at most — whoever is currently awaiting the next
  // message. Chained calls serialise naturally via the promises
  // they return.
  private waiting: Deferred<Incoming> | null = null;

  // Terminal state: a close() was called or the socket errored /
  // hit EOF.
  private closed = false;
  private closeCause: Error | null = null;

  // Dispatch registry: wire typeId → handler + message ctor for
  // decoding. The message ctor is looked up once at registration
  // time so the dispatch path doesn't repeat lookups.
  private readonly handlers = new Map<string, {
    handler: Handler<unknown>;
    ctor: MessageCtor | undefined;
  }>();
  private unknownHandler: Handler<RawBody> | null = null;
  private errorHandler: ((err: unknown) => void | Promise<void>) | null = null;

  // `run()` flag so `close()` can break it out of the loop.
  private running = false;

  // -------------------------------------------------------------
  // Construction
  // -------------------------------------------------------------

  private constructor(socket: Socket, opts: ResolvedClientOptions) {
    this.socket = socket;
    this.opts = opts;
    this.wire();
  }

  /**
   * Open a TCP connection to *host:port* and return a ready
   * {@link Client}. Honours `connectTimeoutMs`. Never returns a
   * half-open handle.
   */
  static async connect(
    host: string,
    port: number,
    options?: ClientOptions,
  ): Promise<Client> {
    const opts = resolveClientOptions(options);
    const socket = await Client.dial(host, port, opts);
    return new Client(socket, opts);
  }

  private static async dial(
    host: string,
    port: number,
    opts: ResolvedClientOptions,
  ): Promise<Socket> {
    return new Promise<Socket>((resolve, reject) => {
      const socket = createConnection({ host, port });
      let settled = false;
      let timer: NodeJS.Timeout | undefined;

      const onConnect = () => {
        if (settled) return;
        settled = true;
        if (timer) clearTimeout(timer);
        socket.off("error", onError);
        resolve(socket);
      };
      const onError = (err: Error) => {
        if (settled) return;
        settled = true;
        if (timer) clearTimeout(timer);
        socket.destroy();
        reject(
          new ConnectionClosedError(
            `connect to ${host}:${port} failed: ${err.message}`,
          ),
        );
      };

      socket.once("connect", onConnect);
      socket.once("error", onError);

      if (opts.connectTimeoutMs !== undefined) {
        timer = setTimeout(() => {
          if (settled) return;
          settled = true;
          socket.destroy();
          reject(
            new TransportTimeoutError(
              `connect to ${host}:${port} timed out after ` +
                `${opts.connectTimeoutMs}ms`,
            ),
          );
        }, opts.connectTimeoutMs);
        // Don't hold the event loop open on this timer.
        timer.unref?.();
      }
    });
  }

  /**
   * Wire up the socket's data/end/error handlers. Called once from
   * the constructor — not re-entrant.
   */
  private wire(): void {
    this.socket.on("data", (chunk: Buffer) => this.onData(chunk));
    this.socket.on("end", () => this.onClose(null));
    this.socket.on("error", (err: Error) => this.onClose(err));
    this.socket.on("close", () => this.onClose(null));
  }

  private onData(chunk: Buffer): void {
    try {
      // Node's Buffer IS a Uint8Array, but wrap defensively so we
      // don't depend on that equivalence.
      this.acc.push(
        new Uint8Array(chunk.buffer, chunk.byteOffset, chunk.byteLength),
      );
      this.drainFrames();
    } catch (err) {
      this.failWith(err);
    }
  }

  private drainFrames(): void {
    while (true) {
      const inc = this.framer.tryParse(this.acc);
      if (inc === null) return;

      // Pre-parse cap. Framer checks its own maxBodySize; we still
      // enforce ours here for the common code path when options
      // cap it (we pass it to the framer, so this is belt+braces).
      if (
        this.opts.maxMessageSize > 0 &&
        inc.header.bodySize > BigInt(this.opts.maxMessageSize)
      ) {
        throw new FramingError(
          `bodySize ${inc.header.bodySize} exceeds maxMessageSize ` +
            `${this.opts.maxMessageSize}`,
        );
      }

      if (this.waiting !== null) {
        const w = this.waiting;
        this.waiting = null;
        w.resolve(inc);
      } else {
        this.ready.push(inc);
      }
    }
  }

  private onClose(err: Error | null): void {
    if (this.closed) return;
    this.closed = true;
    const cause =
      err ?? new ConnectionClosedError("peer closed connection");
    this.closeCause =
      cause instanceof Error
        ? cause
        : new ConnectionClosedError(String(cause));
    if (this.waiting !== null) {
      const w = this.waiting;
      this.waiting = null;
      w.reject(this.closeCause);
    }
  }

  /** Internal: mark the client broken due to an exception. */
  private failWith(err: unknown): void {
    const e =
      err instanceof Error ? err : new ConnectionClosedError(String(err));
    this.closed = true;
    this.closeCause = e;
    try {
      this.socket.destroy();
    } catch {
      // noop
    }
    if (this.waiting !== null) {
      const w = this.waiting;
      this.waiting = null;
      w.reject(e);
    }
  }

  // -------------------------------------------------------------
  // Introspection
  // -------------------------------------------------------------

  get options(): ResolvedClientOptions {
    return this.opts;
  }

  /** `{host, port}` of the remote end, or `null` if disconnected. */
  peer(): { host: string; port: number } | null {
    const addr = this.socket.remoteAddress;
    const port = this.socket.remotePort;
    if (addr === undefined || port === undefined) return null;
    return { host: addr, port };
  }

  get isConnected(): boolean {
    return !this.closed;
  }

  // -------------------------------------------------------------
  // Close
  // -------------------------------------------------------------

  /** Close the connection. Idempotent. */
  async close(): Promise<void> {
    if (this.closed) {
      // Already torn down; nothing to await. Still wait one tick
      // so callers can `await c.close()` without surprise.
      return;
    }
    this.running = false;
    const ended = new Promise<void>((resolve) => {
      this.socket.once("close", () => resolve());
    });
    this.socket.end();
    // Give end-of-stream a chance to flush; fall back to destroy
    // if the peer is slow.
    await Promise.race([ended, delay(500)]);
    if (!this.socket.destroyed) {
      this.socket.destroy();
      await Promise.race([ended, delay(200)]);
    }
  }

  /** TS 5.2+ async disposable — enables `await using c = ...`. */
  async [Symbol.asyncDispose](): Promise<void> {
    await this.close();
  }

  // -------------------------------------------------------------
  // Send
  // -------------------------------------------------------------

  /**
   * Frame and transmit `message`.
   *
   * `deviceName` defaults to `options.defaultDevice`. `timestamp`
   * is the OpenIGTLink 64-bit wire timestamp — callers producing
   * "now" should compute it from their own clock source (this
   * module stays policy-free on time).
   */
  async send(
    message: SendableMessage,
    opts?: { deviceName?: string; timestamp?: bigint },
  ): Promise<void> {
    if (this.closed) {
      throw (
        this.closeCause ?? new ConnectionClosedError("client is closed")
      );
    }
    const ctor = message.constructor as unknown as
      SendableCtor & { name?: string };
    const typeId = ctor.TYPE_ID;
    if (typeof typeId !== "string" || typeId.length === 0) {
      throw new TypeError(
        `${ctor.name ?? "message"} has no TYPE_ID; not a generated ` +
          `OpenIGTLink message class?`,
      );
    }

    const body = message.pack();
    const header = packHeader({
      version: 2,
      typeId,
      deviceName: opts?.deviceName ?? this.opts.defaultDevice,
      timestamp: opts?.timestamp ?? 0n,
      body,
    });

    // `send` concurrency is serialised by the Node socket's write
    // queue — back-to-back writes go out in order even across
    // concurrent callers.
    await new Promise<void>((resolve, reject) => {
      this.socket.write(header, (err) => {
        if (err) return reject(wrapSendError(err));
        this.socket.write(body, (err2) => {
          if (err2) return reject(wrapSendError(err2));
          resolve();
        });
      });
    });
  }

  // -------------------------------------------------------------
  // Receive
  // -------------------------------------------------------------

  /** Receive the next message of any type. */
  async receiveAny(opts?: { timeoutMs?: number }): Promise<Envelope<unknown>> {
    const timeout = opts?.timeoutMs ?? this.opts.receiveTimeoutMs;
    return this.decode(await this.nextIncoming(timeout));
  }

  /**
   * Receive until a message of `ctor` arrives. Intermediate
   * messages that match a registered `on()` handler are dispatched
   * to it; those that don't are dropped. Honours
   * `options.receiveTimeoutMs` or the per-call override.
   */
  async receive<T>(
    ctor: MessageCtor<T>,
    opts?: { timeoutMs?: number },
  ): Promise<Envelope<T>> {
    const typeId = ctor.TYPE_ID;
    const totalBudget = opts?.timeoutMs ?? this.opts.receiveTimeoutMs;
    const deadline =
      totalBudget === undefined ? undefined : Date.now() + totalBudget;

    while (true) {
      const remaining = deadline === undefined ? undefined : deadline - Date.now();
      if (remaining !== undefined && remaining <= 0) {
        throw new TransportTimeoutError(
          `receive(${typeId}) timed out after ${totalBudget}ms`,
        );
      }
      const inc = await this.nextIncoming(remaining);
      if (inc.header.typeId === typeId) {
        // Direct decode with the caller-supplied ctor — avoids the
        // registry lookup since we already know the expected type.
        // CRC already verified by the framer.
        const body = ctor.unpack(inc.body) as T;
        return { header: inc.header, body };
      }
      // Route intermediate types through their handler, if any.
      await this.dispatchOne(inc);
    }
  }

  /**
   * Async iterator over every received message, in order. Ends on
   * peer FIN or `close()`.
   *
   *     for await (const env of c.messages()) { ... }
   */
  async *messages(): AsyncGenerator<Envelope<unknown>> {
    try {
      while (!this.closed || this.ready.length > 0) {
        yield this.decode(await this.nextIncoming(undefined));
      }
    } catch (err) {
      if (err instanceof ConnectionClosedError) return;
      throw err;
    }
  }

  /**
   * Pull the next framed message off the queue, waiting if needed.
   * `timeoutMs` of `undefined` = wait forever.
   */
  private async nextIncoming(
    timeoutMs: number | undefined,
  ): Promise<Incoming> {
    if (this.closed && this.ready.length === 0) {
      throw (
        this.closeCause ?? new ConnectionClosedError("client is closed")
      );
    }
    const queued = this.ready.shift();
    if (queued !== undefined) return queued;

    if (this.waiting !== null) {
      // Programmer error — only one concurrent receive is supported
      // on the simplest client. Chained `await` calls serialise
      // automatically; this guards against two unrelated tasks
      // both awaiting the same client.
      throw new TransportError(
        "another receive() is already in flight; serialise your calls",
      );
    }

    this.waiting = deferred<Incoming>();
    if (timeoutMs === undefined) {
      return this.waiting.promise;
    }

    const timer = setTimeout(() => {
      if (this.waiting !== null) {
        const w = this.waiting;
        this.waiting = null;
        w.reject(
          new TransportTimeoutError(
            `receive timed out after ${timeoutMs}ms`,
          ),
        );
      }
    }, timeoutMs);
    timer.unref?.();
    try {
      return await this.waiting.promise;
    } finally {
      clearTimeout(timer);
    }
  }

  /** Decode an {@link Incoming} into a typed {@link Envelope}.
   *
   * CRC was already verified by the framer that produced `inc`, so
   * we skip the second check. Unknown type_ids degrade to
   * {@link RawBody} via `loose: true` — clients should be resilient
   * to forward-compat message types they don't recognize yet.
   */
  private decode(inc: Incoming): Envelope<unknown> {
    return unpackMessage(inc.header, inc.body, {
      loose: true,
      verifyCrc: false,
    });
  }

  // -------------------------------------------------------------
  // Dispatch
  // -------------------------------------------------------------

  /**
   * Register `handler` for messages of type `ctor`. Returns `this`
   * for chaining. Re-registering replaces the previous handler.
   *
   *     c.on(Transform, async (env) => updatePose(env.body.matrix))
   *      .on(Status,    async (env) => log(env.body.statusMessage));
   */
  on<T>(ctor: MessageCtor<T>, handler: Handler<T>): this {
    this.handlers.set(ctor.TYPE_ID, {
      handler: handler as Handler<unknown>,
      ctor: ctor as MessageCtor,
    });
    return this;
  }

  /** Fallback for messages whose `typeId` has no typed `on()`. */
  onUnknown(handler: Handler<RawBody>): this {
    this.unknownHandler = handler;
    return this;
  }

  /** Called if `run()` or a handler throws. */
  onError(handler: (err: unknown) => void | Promise<void>): this {
    this.errorHandler = handler;
    return this;
  }

  /**
   * Dispatch loop — read messages and route to handlers. Returns
   * when the peer closes or `close()` is called.
   */
  async run(): Promise<void> {
    this.running = true;
    try {
      while (this.running && !this.closed) {
        let inc: Incoming;
        try {
          inc = await this.nextIncoming(undefined);
        } catch (err) {
          if (err instanceof ConnectionClosedError) return;
          if (this.errorHandler) {
            await this.errorHandler(err);
            continue;
          }
          throw err;
        }
        try {
          await this.dispatchOne(inc);
        } catch (err) {
          if (this.errorHandler) {
            await this.errorHandler(err);
          } else {
            throw err;
          }
        }
      }
    } finally {
      this.running = false;
    }
  }

  private async dispatchOne(inc: Incoming): Promise<void> {
    // Decode once through the global REGISTRY so both paths see
    // the same body type. `body` is a typed instance for known
    // type_ids, a RawBody sentinel for unknown ones.
    const env = this.decode(inc);

    const entry = this.handlers.get(inc.header.typeId);
    if (entry !== undefined) {
      await entry.handler(env);
      return;
    }
    if (this.unknownHandler !== null) {
      // Cast: onUnknown's handler type is Handler<RawBody>, but for
      // types that ARE in the registry the body is typed. The
      // handler probably reads `env.header.typeId` anyway — but
      // document that the body type depends on registry membership.
      await this.unknownHandler(env as Envelope<RawBody>);
    }
    // Else silently drop.
  }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function wrapSendError(err: unknown): Error {
  if (err instanceof TransportError) return err;
  const msg = err instanceof Error ? err.message : String(err);
  return new ConnectionClosedError(`send failed: ${msg}`);
}
