/**
 * 58-byte OpenIGTLink message header — parse, emit, and CRC verify.
 *
 * Layout (matches spec/schemas/framing_header.json, C++
 * `core-cpp/include/oigtl/runtime/header.hpp`, and Python
 * `corpus-tools/.../codec/header.py`):
 *
 *     off  size  field
 *       0     2  version (uint16, big-endian)
 *       2    12  type_id (ascii, null-padded)
 *      14    20  device_name (ascii, null-padded)
 *      34     8  timestamp (uint64, big-endian)
 *      42     8  body_size (uint64, big-endian)
 *      50     8  crc (uint64, big-endian, ECMA-182 over body)
 *     total: 58
 *
 * The header is invariant across protocol versions — parsed before
 * the content schema is known.
 *
 * Note: `timestamp` and `body_size` are `bigint`. The protocol
 * defines them as `uint64` and JavaScript `number` loses precision
 * above 2^53. `version` is `number` — the spec caps it at 3.
 */

import { readU16, readU64, viewOf, writeU16, writeU64 } from "./byte_order.js";
import { crc64 } from "./crc64.js";
import { CrcMismatchError, HeaderParseError } from "./errors.js";

export const HEADER_SIZE = 58;
export const TYPE_ID_SIZE = 12;
export const DEVICE_NAME_SIZE = 20;

export interface Header {
  /** Protocol version: 1 (v1/v2 body-only), 2 (v2 with ext header), 3 (v3). */
  version: number;
  /** Up to 12 ASCII chars, trailing NULs stripped. */
  typeId: string;
  /** Up to 20 ASCII chars, trailing NULs stripped. */
  deviceName: string;
  /** Seconds-since-epoch in the upper 32 bits, fractional in the lower 32. */
  timestamp: bigint;
  /** Length of the body region that follows the header. */
  bodySize: bigint;
  /** CRC-64 ECMA-182 over the body, as declared in the header. */
  crc: bigint;
}

// ---------------------------------------------------------------------------
// Parse
// ---------------------------------------------------------------------------

/**
 * Parse a 58-byte header at the start of `bytes`.
 *
 * Throws {@link HeaderParseError} if fewer than 58 bytes are
 * available. Does NOT verify CRC — call {@link verifyCrc} after
 * slicing the body.
 */
export function unpackHeader(bytes: Uint8Array): Header {
  if (bytes.length < HEADER_SIZE) {
    throw new HeaderParseError(
      `need ${HEADER_SIZE} bytes for a header, got ${bytes.length}`,
    );
  }
  const view = viewOf(bytes);
  const version = readU16(view, 0);
  if (version !== 1 && version !== 2 && version !== 3) {
    throw new HeaderParseError(
      `header version=${version} is not in the supported set {1, 2, 3}`,
    );
  }
  const typeId = _readAsciiNullPadded(bytes, 2, TYPE_ID_SIZE);
  const deviceName = _readAsciiNullPadded(bytes, 14, DEVICE_NAME_SIZE);
  const timestamp = readU64(view, 34);
  const bodySize = readU64(view, 42);
  const crc = readU64(view, 50);
  return { version, typeId, deviceName, timestamp, bodySize, crc };
}

function _readAsciiNullPadded(bytes: Uint8Array, offset: number, size: number): string {
  let end = offset + size;
  // Trim trailing NULs.
  while (end > offset && bytes[end - 1] === 0) end--;
  let out = "";
  for (let i = offset; i < end; i++) out += String.fromCharCode(bytes[i] as number);
  return out;
}

function _writeAsciiNullPadded(
  bytes: Uint8Array,
  offset: number,
  size: number,
  value: string,
  fieldName: string,
): void {
  if (value.length > size) {
    throw new HeaderParseError(
      `${fieldName} too long: ${value.length} chars > ${size} byte limit`,
    );
  }
  for (let i = 0; i < value.length; i++) {
    const code = value.charCodeAt(i);
    if (code > 0x7f) {
      throw new HeaderParseError(
        `${fieldName} is ASCII-only; got char 0x${code.toString(16)} at ${i}`,
      );
    }
    bytes[offset + i] = code;
  }
  // The remaining bytes are already 0 in a freshly-allocated Uint8Array.
}

// ---------------------------------------------------------------------------
// Emit
// ---------------------------------------------------------------------------

export interface PackHeaderOpts {
  version: number;
  typeId: string;
  deviceName: string;
  timestamp: bigint;
  /** The body bytes. CRC is computed over this buffer. */
  body: Uint8Array;
}

/**
 * Serialize a header into a fresh 58-byte Uint8Array. Computes
 * CRC-64 over `body` and embeds it; sets `body_size` to
 * `body.length`.
 */
export function packHeader(opts: PackHeaderOpts): Uint8Array {
  const out = new Uint8Array(HEADER_SIZE);
  const view = viewOf(out);
  writeU16(view, 0, opts.version);
  _writeAsciiNullPadded(out, 2, TYPE_ID_SIZE, opts.typeId, "type_id");
  _writeAsciiNullPadded(out, 14, DEVICE_NAME_SIZE, opts.deviceName, "device_name");
  writeU64(view, 34, opts.timestamp);
  writeU64(view, 42, BigInt(opts.body.length));
  writeU64(view, 50, crc64(opts.body));
  return out;
}

// ---------------------------------------------------------------------------
// Verify
// ---------------------------------------------------------------------------

/** Throws {@link CrcMismatchError} if `crc64(body)` ≠ `header.crc`. */
export function verifyCrc(header: Header, body: Uint8Array): void {
  const actual = crc64(body);
  if (actual !== header.crc) {
    throw new CrcMismatchError(header.crc, actual);
  }
}
