/**
 * v3 framer — peel one message off a stream of already-accumulated
 * bytes, or wrap an outbound packed message for the wire.
 *
 * The default framer is the only one we ship today. A future v4
 * streaming/multiplexed framer would be a different implementation
 * of the same shape; the `Client` stays identical.
 *
 * Matches the C++ `make_v3_framer` in
 * `core-cpp/src/transport/framer_v3.cpp` and the Python
 * `V3Framer` in `core-py/src/oigtl/net/framer.py`.
 */

import { crc64 } from "../runtime/crc64.js";
import { CrcMismatchError } from "../runtime/errors.js";
import {
  HEADER_SIZE,
  type Header,
  unpackHeader,
} from "../runtime/header.js";
import { FramingError } from "./errors.js";

/**
 * One parsed wire message: the 58-byte header plus the body bytes
 * it announced, already CRC-verified.
 */
export interface Incoming {
  readonly header: Header;
  readonly body: Uint8Array;
}

/**
 * Accumulating byte buffer that keeps framer state across
 * partial-read boundaries. Built as a simple growable Uint8Array
 * wrapper rather than depending on Node's `Buffer` so the code
 * stays portable (the WebSocket transport in a later phase will
 * reuse this class unchanged in the browser).
 */
export class ByteAccumulator {
  private buf: Uint8Array;
  private len = 0;

  constructor(initialCapacity = 4096) {
    this.buf = new Uint8Array(initialCapacity);
  }

  get length(): number {
    return this.len;
  }

  /** Append `chunk`; grow if needed. */
  push(chunk: Uint8Array): void {
    const need = this.len + chunk.length;
    if (need > this.buf.length) {
      // Double until we fit — standard amortised-O(1) growth.
      let cap = this.buf.length;
      while (cap < need) cap *= 2;
      const grown = new Uint8Array(cap);
      grown.set(this.buf.subarray(0, this.len));
      this.buf = grown;
    }
    this.buf.set(chunk, this.len);
    this.len += chunk.length;
  }

  /** Zero-copy view over the current contents. Invalidated by `push` / `consume`. */
  peek(): Uint8Array {
    return this.buf.subarray(0, this.len);
  }

  /** Drop the first `n` bytes (shift tail left). */
  consume(n: number): void {
    if (n < 0 || n > this.len) {
      throw new RangeError(`consume(${n}) out of bounds (len=${this.len})`);
    }
    this.buf.copyWithin(0, n, this.len);
    this.len -= n;
  }
}

/**
 * Default v3 framer: 58-byte header + `bodySize` body, no envelope.
 *
 * `maxBodySize` of 0 means no additional cap (the 64-bit wire
 * field itself still bounds `bodySize`). A non-zero cap is
 * enforced BEFORE body bytes are required — a peer announcing a
 * huge `bodySize` is rejected immediately, not after waiting for
 * bytes.
 */
export class V3Framer {
  readonly name = "v3";
  private readonly maxBodySize: bigint;

  constructor(maxBodySize = 0) {
    if (maxBodySize < 0) {
      throw new RangeError("maxBodySize must be >= 0");
    }
    this.maxBodySize = BigInt(maxBodySize);
  }

  /**
   * Attempt to peel one message off the front of `buf`. On success,
   * consumes the prefix and returns the {@link Incoming}. On "not
   * enough bytes yet" returns `null` and leaves `buf` untouched.
   * On malformed bytes, throws.
   */
  tryParse(buf: ByteAccumulator): Incoming | null {
    if (buf.length < HEADER_SIZE) return null;

    const view = buf.peek();
    const header = unpackHeader(view);

    // Pre-parse cap — enforced BEFORE we wait for body bytes.
    if (this.maxBodySize > 0n && header.bodySize > this.maxBodySize) {
      throw new FramingError(
        `bodySize ${header.bodySize} exceeds configured maxMessageSize ` +
          `${this.maxBodySize}`,
      );
    }

    const bodySizeN = Number(header.bodySize);
    if (!Number.isSafeInteger(bodySizeN)) {
      // A bodySize above Number.MAX_SAFE_INTEGER is not something
      // we can allocate in-process anyway — this is DoS territory.
      throw new FramingError(
        `bodySize ${header.bodySize} exceeds Number.MAX_SAFE_INTEGER`,
      );
    }

    const total = HEADER_SIZE + bodySizeN;
    if (buf.length < total) return null;

    // Re-slice now that we know we have all the bytes. `slice()`
    // rather than `subarray()` so the Incoming outlives subsequent
    // `buf.consume()` calls that shift bytes around in the backing
    // store.
    const wire = buf.peek();
    const body = wire.slice(HEADER_SIZE, total);

    const actual = crc64(body);
    if (actual !== header.crc) {
      throw new CrcMismatchError(header.crc, actual);
    }

    buf.consume(total);
    return { header, body };
  }

  /** Wrap an outbound packed wire message for the wire. v3: identity. */
  frame(wire: Uint8Array): Uint8Array {
    return wire;
  }
}

export function makeV3Framer(maxBodySize = 0): V3Framer {
  return new V3Framer(maxBodySize);
}
