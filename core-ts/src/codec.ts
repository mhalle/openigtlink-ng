/**
 * Pure (transport-independent) OpenIGTLink wire codec.
 *
 * Sits above the header codec ({@link unpackHeader}/{@link packHeader})
 * and the message-type registry ({@link lookupMessageClass}), and
 * below every transport (`Client`, `WsClient`, future MQTT adapter,
 * file replay, notebook consumers).
 *
 * The four-step pattern
 * ---------------------
 *
 * Decoding one wire message follows the same four steps regardless
 * of transport:
 *
 *   1. **read_header_bytes**  — I/O. Read exactly 58 bytes.
 *   2. **unpackHeader**       — pure. Parse them into a {@link Header}.
 *   3. **read_message_bytes** — I/O. Read `header.bodySize` more bytes.
 *   4. **unpackMessage**      — pure. Parse (header, body) into an Envelope.
 *
 * Steps 2 and 4 are the two pure entry points this module exposes.
 * Callers that already have a *complete* wire message in memory (MQTT
 * payload, file slice, unit-test fixture, WebSocket binary frame)
 * can skip the interleaving and call {@link unpackEnvelope} in one
 * shot.
 *
 * The inverse (encode) has no data dependency, so it's two functions
 * not three: {@link packEnvelope} handles the whole message, and
 * {@link packHeader} stays available for the occasional caller that
 * needs only the 58-byte header.
 *
 * Extensions
 * ----------
 *
 * Any class satisfying the {@link MessageCtor} shape — a static
 * `TYPE_ID` string, a static `unpack(Uint8Array)` factory, and an
 * instance `pack(): Uint8Array` — can be made decodable via
 * {@link registerMessageType}. Built-in types and extensions share
 * one dispatch path.
 */

import type { Envelope } from "./net/envelope.js";
import { RawBody } from "./net/envelope.js";
import {
  lookupMessageClass,
  type MessageCtor,
} from "./runtime/dispatch.js";
import {
  CrcMismatchError,
  HeaderParseError,
  MalformedMessageError,
  ShortBufferError,
  UnknownMessageTypeError,
} from "./runtime/errors.js";
import {
  HEADER_SIZE,
  type Header,
  packHeader,
  unpackHeader,
  verifyCrc,
} from "./runtime/header.js";

export {
  HEADER_SIZE,
  type Header,
  packHeader,
  unpackHeader,
} from "./runtime/header.js";
export { RawBody, type Envelope } from "./net/envelope.js";

/**
 * Options shared by every `unpack*` entry point.
 */
export interface UnpackOptions {
  /**
   * If true, unknown `typeId` values produce an Envelope whose body
   * is {@link RawBody}. If false (default), unknown types throw
   * {@link UnknownMessageTypeError}.
   */
  loose?: boolean;
  /**
   * If true (default), verifies CRC-64 against the header's declared
   * value. Set false when an upstream layer (e.g. the framer) has
   * already verified, or for bulk replay of pre-verified fixtures.
   */
  verifyCrc?: boolean;
}

/**
 * Decode one message's body and pair it with `header`.
 *
 * This is step 4 of the four-step pattern: callers that have just
 * read (or sliced out) exactly `header.bodySize` body bytes use this
 * to produce the final typed {@link Envelope}.
 *
 * @throws {MalformedMessageError} `body.length` doesn't match
 *   `header.bodySize`.
 * @throws {CrcMismatchError} `verifyCrc !== false` and the CRC
 *   doesn't match.
 * @throws {UnknownMessageTypeError} `loose !== true` and no body
 *   class is registered for `header.typeId`.
 */
export function unpackMessage(
  header: Header,
  body: Uint8Array,
  opts: UnpackOptions = {},
): Envelope<unknown> {
  const loose = opts.loose ?? false;
  const verify = opts.verifyCrc ?? true;

  if (BigInt(body.length) !== header.bodySize) {
    throw new MalformedMessageError(
      `body length ${body.length} does not match ` +
        `header.bodySize ${header.bodySize}`,
    );
  }

  if (verify) {
    verifyCrc(header, body);
  }

  // v1 messages carry the body content verbatim. v2/v3 wrap it in
  // [12-byte extended header | content | metadata]; the body class
  // expects only the content slice. Peel the ext header + the
  // trailing metadata region off before dispatching.
  const content = _extractContent(header, body);

  const ctor = lookupMessageClass(header.typeId);
  if (ctor !== undefined) {
    return { header, body: ctor.unpack(content) };
  }

  if (loose) {
    // RawBody keeps the *original* body bytes (ext-header + metadata
    // intact) so gateway code that wants to re-emit the wire
    // untouched still works.
    return { header, body: new RawBody(header.typeId, body) };
  }

  throw new UnknownMessageTypeError(header.typeId);
}

// Minimum v2 extended-header size. Matches the constant in
// runtime/header.ts (kept local here to avoid a cycle).
const V2_EXT_HEADER_MIN_SIZE = 12;

/**
 * Return the content slice of `body` for this header version.
 *
 * - v1: body == content, returned as-is (a subarray view over the
 *   same memory, not a copy).
 * - v2/v3: [ext_header | content | metadata] — strip both ends.
 *
 * @throws {MalformedMessageError} v2/v3 framing fields declare
 *   region sizes that don't fit in the available body bytes.
 */
function _extractContent(header: Header, body: Uint8Array): Uint8Array {
  if (header.version < 2) {
    return body;
  }
  if (body.length < V2_EXT_HEADER_MIN_SIZE) {
    throw new MalformedMessageError(
      `v${header.version} body too short for extended header: ` +
        `${body.length} < ${V2_EXT_HEADER_MIN_SIZE}`,
    );
  }
  const view = new DataView(body.buffer, body.byteOffset, body.byteLength);
  const extHeaderSize = view.getUint16(0, false);
  const metaHeaderSize = view.getUint16(2, false);
  const metaSize = view.getUint32(4, false);

  if (
    extHeaderSize < V2_EXT_HEADER_MIN_SIZE ||
    extHeaderSize > body.length
  ) {
    throw new MalformedMessageError(
      `bogus ext_header_size ${extHeaderSize} (body is ${body.length} bytes)`,
    );
  }
  const metadataTotal = metaHeaderSize + metaSize;
  if (metadataTotal > body.length - extHeaderSize) {
    throw new MalformedMessageError(
      `declared metadata region (${metadataTotal} bytes) exceeds ` +
        `remaining body after ext header ` +
        `(${body.length - extHeaderSize} bytes)`,
    );
  }
  const contentEnd = body.length - metadataTotal;
  return body.subarray(extHeaderSize, contentEnd);
}

/**
 * Decode a complete wire message (header + body) in one call.
 *
 * Convenience for callers who already hold the full bytes of one
 * message in memory — MQTT payloads, file slices, WebSocket binary
 * frames, unit-test fixtures. Streaming transports (TCP via the
 * framer) use the two-step pair {@link unpackHeader} +
 * {@link unpackMessage} instead, because they can't know `bodySize`
 * ahead of time.
 *
 * The `wire` buffer must be exactly `HEADER_SIZE + header.bodySize`
 * bytes long; extra trailing bytes are treated as framing corruption
 * and throw {@link MalformedMessageError}.
 *
 * @throws {ShortBufferError} `wire` is shorter than one full message.
 * @throws {MalformedMessageError} `wire` is longer than exactly one
 *   message (trailing garbage).
 * @throws See exceptions documented on {@link unpackMessage}.
 */
export function unpackEnvelope(
  wire: Uint8Array,
  opts: UnpackOptions = {},
): Envelope<unknown> {
  if (wire.length < HEADER_SIZE) {
    throw new ShortBufferError(
      `wire shorter than header: ${wire.length} < ${HEADER_SIZE}`,
    );
  }

  let header: Header;
  try {
    header = unpackHeader(wire.subarray(0, HEADER_SIZE));
  } catch (e) {
    if (e instanceof HeaderParseError) {
      throw new MalformedMessageError(e.message);
    }
    throw e;
  }

  const bodySize = Number(header.bodySize);
  const expected = HEADER_SIZE + bodySize;
  if (wire.length < expected) {
    throw new ShortBufferError(
      `wire truncated: header declares bodySize=${header.bodySize} ` +
        `(needs ${expected} total), got ${wire.length}`,
    );
  }
  if (wire.length > expected) {
    throw new MalformedMessageError(
      `wire has trailing bytes: header declares bodySize=${header.bodySize} ` +
        `(expected ${expected} total), got ${wire.length}`,
    );
  }

  const body = wire.subarray(HEADER_SIZE, expected);
  return unpackMessage(header, body, opts);
}

/**
 * Serialize `envelope` to its complete wire representation.
 *
 * Round-trips with {@link unpackEnvelope}: for any Envelope
 * produced by `unpackEnvelope(w)`, `packEnvelope(env)` returns
 * wire bytes that byte-compare equal to `w`.
 *
 * The CRC carried in the header is **recomputed** from the body.
 * An envelope loaded with `verifyCrc: false` and whose `header.crc`
 * has intentionally been left stale will therefore round trip to a
 * *different* header on re-pack — the canonical CRC wins. This is
 * the correct behaviour for a serialiser; it keeps the produced
 * bytes internally consistent regardless of where the envelope came
 * from.
 *
 * The body bytes come from the body object's `pack()` method for
 * typed messages, or from `body.bytes` verbatim for {@link RawBody}
 * fallbacks. Both paths reproduce the original wire body bit-for-bit.
 *
 * @throws If the body isn't a RawBody and has no `pack()` method —
 *   means the caller constructed the envelope with a non-message
 *   object or a registered class missing the expected contract.
 */
export function packEnvelope(envelope: Envelope<unknown>): Uint8Array {
  const { header, body } = envelope;

  let bodyBytes: Uint8Array;
  if (body instanceof RawBody) {
    bodyBytes = body.bytes;
  } else {
    const maybePack = (body as { pack?: () => Uint8Array }).pack;
    if (typeof maybePack !== "function") {
      throw new TypeError(
        `envelope body has no pack() method; got ${typeof body}`,
      );
    }
    bodyBytes = maybePack.call(body);
  }

  const headerBytes = packHeader({
    version: header.version,
    typeId: header.typeId,
    deviceName: header.deviceName,
    timestamp: header.timestamp,
    body: bodyBytes,
  });

  const out = new Uint8Array(headerBytes.length + bodyBytes.length);
  out.set(headerBytes, 0);
  out.set(bodyBytes, headerBytes.length);
  return out;
}

// Re-export registration surface so extension authors have one import:
//   import { registerMessageType } from "@openigtlink/core/codec";
export {
  RegistryConflictError,
  lookupMessageClass,
  registerMessageType,
  registeredTypes,
  registrySize,
  unregisterMessageType,
  type MessageCtor,
} from "./runtime/dispatch.js";
