/**
 * Async OpenIGTLink client over WebSocket.
 *
 * Parallels the Node-TCP {@link Client} in shape (connect, send,
 * receive, dispatch, messages) but uses WebSocket framing — one
 * binary frame = one IGTL message. No `V3Framer`, no
 * `ByteAccumulator`; the WS layer already delimits messages.
 *
 * Browser-safe: imports nothing from Node. Uses `globalThis.WebSocket`
 * by default; pass `{ webSocket: SomeImpl }` to override (e.g. the
 * `ws` npm package for Node 20/21 where native WebSocket isn't
 * available).
 *
 * No WSS/TLS yet. `wss://` URLs work at runtime (the browser
 * WebSocket API handles TLS), but we haven't tested that path.
 *
 *     import { WsClient } from "@openigtlink/core/net/ws";
 *     import { Transform, Status } from "@openigtlink/core/messages";
 *
 *     const c = await WsClient.connect("ws://tracker.lab:18945/");
 *     await c.send(new Transform({ matrix: [...] }));
 *     const reply = await c.receive(Status);
 *     await c.close();
 */

import { extractContent, unpackMessage } from "../codec.js";
import { type MessageCtor } from "../runtime/dispatch.js";
import { packHeader, unpackHeader, HEADER_SIZE } from "../runtime/header.js";
import { crc64 } from "../runtime/crc64.js";
import { CrcMismatchError } from "../runtime/errors.js";
import {
  ConnectionClosedError,
  FramingError,
  TransportError,
  TransportTimeoutError,
} from "./errors.js";
import { RawBody, type Envelope } from "./envelope.js";
import type {
  ClientOptions,
  ResolvedClientOptions,
} from "./options.js";
import { resolveClientOptions } from "./options.js";
import type {
  Handler,
  SendableCtor,
  SendableMessage,
} from "./types.js";

export type { Handler, SendableCtor, SendableMessage } from "./types.js";

// ---------------------------------------------------------------------------
// Runtime WebSocket shim
// ---------------------------------------------------------------------------

/**
 * Minimal shape we require from a WebSocket implementation.
 * The browser `WebSocket` class and the Node `ws` package's
 * `WebSocket` class both satisfy this — we only use the standard
 * Web-IDL surface.
 */
export interface WebSocketLike {
  readonly readyState: number;
  binaryType: "arraybuffer" | "blob" | "nodebuffer" | string;
  send(data: ArrayBuffer | Uint8Array): void;
  close(code?: number, reason?: string): void;
  addEventListener(
    type: "open" | "close" | "error" | "message",
    listener: (event: any) => void,  // eslint-disable-line @typescript-eslint/no-explicit-any
  ): void;
}

export interface WebSocketLikeCtor {
  new (url: string): WebSocketLike;
  readonly CONNECTING: number;
  readonly OPEN: number;
  readonly CLOSING: number;
  readonly CLOSED: number;
}

/** Options specific to WsClient — extends the shared ClientOptions. */
export interface WsClientOptions extends ClientOptions {
  /**
   * WebSocket constructor to use. Defaults to `globalThis.WebSocket`,
   * which works in browsers and Node 22+. For Node 20/21, install
   * the `ws` npm package and pass `{ webSocket: WebSocket }` where
   * `WebSocket` is `(await import("ws")).WebSocket`.
   */
  webSocket?: WebSocketLikeCtor;
}

function resolveWebSocketCtor(
  override?: WebSocketLikeCtor,
): WebSocketLikeCtor {
  if (override !== undefined) return override;
  const g = globalThis as unknown as {
    WebSocket?: WebSocketLikeCtor;
  };
  if (g.WebSocket !== undefined) return g.WebSocket;
  throw new TransportError(
    "No WebSocket constructor available. Browsers and Node 22+ have " +
      "one natively; for Node 20/21, pass `{ webSocket: ... }` from " +
      "the `ws` npm package.",
  );
}

// ---------------------------------------------------------------------------
// Deferred primitive (shared with TCP Client idiom; duplicated to
// keep the WS module browser-safe — no Node imports).
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
// WsClient
// ---------------------------------------------------------------------------

interface Incoming {
  header: ReturnType<typeof unpackHeader>;
  body: Uint8Array;
}

export class WsClient {
  private readonly ws: WebSocketLike;
  private readonly _url: string;
  private readonly opts: ResolvedClientOptions;

  private readonly ready: Incoming[] = [];
  private waiting: Deferred<Incoming> | null = null;

  private closed = false;
  private closeCause: Error | null = null;

  private readonly handlers = new Map<string, {
    handler: Handler<unknown>;
    ctor: MessageCtor | undefined;
  }>();
  private unknownHandler: Handler<RawBody> | null = null;
  private errorHandler: ((err: unknown) => void | Promise<void>) | null = null;
  private running = false;

  private constructor(
    ws: WebSocketLike,
    url: string,
    opts: ResolvedClientOptions,
  ) {
    this.ws = ws;
    this._url = url;
    this.opts = opts;
    this.wire();
  }

  // -------------------------------------------------------------
  // Construction
  // -------------------------------------------------------------

  static async connect(
    url: string,
    options?: WsClientOptions,
  ): Promise<WsClient> {
    if (!url.startsWith("ws://") && !url.startsWith("wss://")) {
      throw new Error(
        `ws URL must start with ws:// or wss://; got "${url}"`,
      );
    }
    const opts = resolveClientOptions(options);
    const WS = resolveWebSocketCtor(options?.webSocket);

    const ws = new WS(url);
    // Binary frames must arrive as ArrayBuffer so we can work with
    // them uniformly in browser + Node. "nodebuffer" (Node ws
    // default) gives us Buffer; "arraybuffer" gives us ArrayBuffer.
    ws.binaryType = "arraybuffer";

    await new Promise<void>((resolve, reject) => {
      let settled = false;
      let timer: ReturnType<typeof setTimeout> | undefined;

      const onOpen = () => {
        if (settled) return;
        settled = true;
        if (timer) clearTimeout(timer);
        resolve();
      };
      const onError = (ev: unknown) => {
        if (settled) return;
        settled = true;
        if (timer) clearTimeout(timer);
        try { ws.close(); } catch { /* noop */ }
        reject(
          new ConnectionClosedError(
            `ws connect to ${url} failed: ${errorMessage(ev)}`,
          ),
        );
      };
      const onClose = (ev: unknown) => {
        if (settled) return;
        settled = true;
        if (timer) clearTimeout(timer);
        reject(
          new ConnectionClosedError(
            `ws connect to ${url} failed during handshake: ${
              errorMessage(ev)
            }`,
          ),
        );
      };

      ws.addEventListener("open", onOpen);
      ws.addEventListener("error", onError);
      ws.addEventListener("close", onClose);

      if (opts.connectTimeoutMs !== undefined) {
        timer = setTimeout(() => {
          if (settled) return;
          settled = true;
          try { ws.close(); } catch { /* noop */ }
          reject(
            new TransportTimeoutError(
              `ws connect to ${url} timed out after ${opts.connectTimeoutMs}ms`,
            ),
          );
        }, opts.connectTimeoutMs);
        (timer as { unref?: () => void }).unref?.();
      }
    });

    return new WsClient(ws, url, opts);
  }

  private wire(): void {
    this.ws.addEventListener("message", (ev: { data: unknown }) => {
      try {
        this.onMessage(ev.data);
      } catch (err) {
        this.failWith(err);
      }
    });
    this.ws.addEventListener("close", () => this.onClose(null));
    this.ws.addEventListener("error", (ev: unknown) => {
      this.onClose(
        new ConnectionClosedError(
          `ws error: ${errorMessage(ev)}`,
        ),
      );
    });
  }

  private onMessage(data: unknown): void {
    // Browser: data is ArrayBuffer (binaryType="arraybuffer") or string.
    // Node ws: data is ArrayBuffer because we set binaryType above.
    if (typeof data === "string") {
      throw new FramingError(
        "received text WebSocket frame; OIGTL requires binary",
      );
    }
    let bytes: Uint8Array;
    if (data instanceof ArrayBuffer) {
      bytes = new Uint8Array(data);
    } else if (ArrayBuffer.isView(data)) {
      const v = data as ArrayBufferView;
      bytes = new Uint8Array(v.buffer, v.byteOffset, v.byteLength);
    } else {
      throw new FramingError(
        `unexpected frame payload type: ${Object.prototype.toString.call(data)}`,
      );
    }

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
    const expected = HEADER_SIZE + bodySize;
    if (bytes.length !== expected) {
      throw new FramingError(
        `WS frame size ${bytes.length} does not match declared ` +
          `bodySize=${bodySize} (expected ${expected})`,
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

    const body = bytes.slice(HEADER_SIZE);
    const actual = crc64(body);
    if (actual !== header.crc) {
      throw new CrcMismatchError(header.crc, actual);
    }

    const inc: Incoming = { header, body };
    if (this.waiting !== null) {
      const w = this.waiting;
      this.waiting = null;
      w.resolve(inc);
    } else {
      this.ready.push(inc);
    }
  }

  private onClose(err: Error | null): void {
    if (this.closed) return;
    this.closed = true;
    const cause = err ?? new ConnectionClosedError("peer closed connection");
    this.closeCause = cause;
    if (this.waiting !== null) {
      const w = this.waiting;
      this.waiting = null;
      w.reject(this.closeCause);
    }
  }

  private failWith(err: unknown): void {
    const e =
      err instanceof Error ? err : new ConnectionClosedError(String(err));
    this.closed = true;
    this.closeCause = e;
    try { this.ws.close(); } catch { /* noop */ }
    if (this.waiting !== null) {
      const w = this.waiting;
      this.waiting = null;
      w.reject(e);
    }
  }

  // -------------------------------------------------------------
  // Introspection
  // -------------------------------------------------------------

  get url(): string { return this._url; }
  get options(): ResolvedClientOptions { return this.opts; }
  get isConnected(): boolean { return !this.closed; }

  // -------------------------------------------------------------
  // Close
  // -------------------------------------------------------------

  async close(): Promise<void> {
    if (this.closed) return;
    this.running = false;
    const ended = new Promise<void>((resolve) => {
      this.ws.addEventListener("close", () => resolve());
    });
    try { this.ws.close(); } catch { /* noop */ }
    // Budget the drain so a misbehaving peer doesn't hang close().
    await Promise.race([ended, timeoutPromise(500)]);
  }

  async [Symbol.asyncDispose](): Promise<void> {
    await this.close();
  }

  // -------------------------------------------------------------
  // Send
  // -------------------------------------------------------------

  async send(
    message: SendableMessage,
    opts?: { deviceName?: string; timestamp?: bigint },
  ): Promise<void> {
    if (this.closed) {
      throw this.closeCause ?? new ConnectionClosedError("client is closed");
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
    // v1 framing — see the matching comment in src/net/client.ts.
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

    try {
      this.ws.send(wire);
    } catch (err) {
      this.failWith(err);
      throw new ConnectionClosedError(`ws send failed: ${errorMessage(err)}`);
    }
  }

  // -------------------------------------------------------------
  // Receive
  // -------------------------------------------------------------

  async receiveAny(opts?: { timeoutMs?: number }): Promise<Envelope<unknown>> {
    const timeout = opts?.timeoutMs ?? this.opts.receiveTimeoutMs;
    return this.decode(await this.nextIncoming(timeout));
  }

  async receive<T>(
    ctor: MessageCtor<T>,
    opts?: { timeoutMs?: number },
  ): Promise<Envelope<T>> {
    const typeId = ctor.TYPE_ID;
    const total = opts?.timeoutMs ?? this.opts.receiveTimeoutMs;
    const deadline = total === undefined ? undefined : Date.now() + total;

    while (true) {
      const remaining = deadline === undefined ? undefined : deadline - Date.now();
      if (remaining !== undefined && remaining <= 0) {
        throw new TransportTimeoutError(
          `receive(${typeId}) timed out after ${total}ms`,
        );
      }
      const inc = await this.nextIncoming(remaining);
      if (inc.header.typeId === typeId) {
        // Strip v2/v3 ext_header + metadata before ctor.unpack;
        // same reasoning as Client.receive.
        const content = extractContent(inc.header, inc.body);
        return { header: inc.header, body: ctor.unpack(content) as T };
      }
      await this.dispatchOne(inc);
    }
  }

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

  private async nextIncoming(
    timeoutMs: number | undefined,
  ): Promise<Incoming> {
    if (this.closed && this.ready.length === 0) {
      throw this.closeCause ?? new ConnectionClosedError("client is closed");
    }
    const queued = this.ready.shift();
    if (queued !== undefined) return queued;

    if (this.waiting !== null) {
      throw new TransportError(
        "another receive() is already in flight; serialise your calls",
      );
    }
    this.waiting = deferred<Incoming>();
    if (timeoutMs === undefined) return this.waiting.promise;

    const timer = setTimeout(() => {
      if (this.waiting !== null) {
        const w = this.waiting;
        this.waiting = null;
        w.reject(
          new TransportTimeoutError(`receive timed out after ${timeoutMs}ms`),
        );
      }
    }, timeoutMs);
    (timer as { unref?: () => void }).unref?.();
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
   * {@link RawBody} via `loose: true`.
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

  on<T>(ctor: MessageCtor<T>, handler: Handler<T>): this {
    this.handlers.set(ctor.TYPE_ID, {
      handler: handler as Handler<unknown>,
      ctor: ctor as MessageCtor,
    });
    return this;
  }

  onUnknown(handler: Handler<RawBody>): this {
    this.unknownHandler = handler;
    return this;
  }

  onError(handler: (err: unknown) => void | Promise<void>): this {
    this.errorHandler = handler;
    return this;
  }

  async run(): Promise<void> {
    this.running = true;
    try {
      while (this.running && !this.closed) {
        let inc: Incoming;
        try {
          inc = await this.nextIncoming(undefined);
        } catch (err) {
          if (err instanceof ConnectionClosedError) return;
          if (this.errorHandler) { await this.errorHandler(err); continue; }
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
    const env = this.decode(inc);
    const entry = this.handlers.get(inc.header.typeId);
    if (entry !== undefined) {
      await entry.handler(env);
      return;
    }
    if (this.unknownHandler !== null) {
      await this.unknownHandler(env as Envelope<RawBody>);
    }
  }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function errorMessage(ev: unknown): string {
  if (ev instanceof Error) return ev.message;
  if (ev && typeof ev === "object" && "message" in ev) {
    return String((ev as { message: unknown }).message);
  }
  return String(ev);
}

function timeoutPromise(ms: number): Promise<void> {
  return new Promise((resolve) => {
    const t = setTimeout(resolve, ms);
    (t as { unref?: () => void }).unref?.();
  });
}
