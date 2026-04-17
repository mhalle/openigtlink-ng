// GENERATED from spec/schemas/header.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen ts
/**
 * HEADER — typed TypeScript wire codec.
 */

import {
  readU8, readI8, readU16, readI16, readU32, readI32,
  readU64, readI64, readF32, readF64,
  writeU8, writeI8, writeU16, writeI16, writeU32, writeI32,
  writeU64, writeI64, writeF32, writeF64,
  readU16ArrayBE, readI16ArrayBE, readU32ArrayBE, readI32ArrayBE,
  readU64ArrayBE, readI64ArrayBE, readF32ArrayBE, readF64ArrayBE,
  writeU16ArrayBE, writeI16ArrayBE, writeU32ArrayBE, writeI32ArrayBE,
  writeU64ArrayBE, writeI64ArrayBE, writeF32ArrayBE, writeF64ArrayBE,
  viewOf,
} from "../runtime/byte_order.js";
import { BodyDecodeError } from "../runtime/errors.js";

// Strict ASCII helpers used for fixed_string fields, struct-array
// sub-fields, and the encoding="ascii" variants of
// length_prefixed_string / trailing_string.
//
// Both helpers reject any byte >= 0x80, matching the reference
// Python codec's `bytes.decode("ascii")`. Mixing ASCII-strict and
// permissive decoders across codecs caused 24 of the 32
// disagreements in the first 100k differential-fuzz run; strict
// everywhere closes the class.
function _readAscii(bytes: Uint8Array, offset: number, size: number): string {
  // Null-padded: stop at the first NUL byte, discard the rest.
  // Bytes before the first NUL must be ASCII (< 0x80).
  const fieldEnd = offset + size;
  let end = offset;
  while (end < fieldEnd && bytes[end] !== 0) end++;
  let out = "";
  for (let i = offset; i < end; i++) {
    const b = bytes[i] as number;
    if (b >= 0x80) {
      throw new BodyDecodeError(
        `non-ASCII byte 0x${b.toString(16)} at content offset ${i}`,
      );
    }
    out += String.fromCharCode(b);
  }
  return out;
}

function _readAsciiRaw(bytes: Uint8Array, offset: number, size: number): string {
  // Exact-size: read all `size` bytes, no NUL stripping. Used for
  // null_padded=false fields and for length_prefixed / trailing
  // strings carrying encoding="ascii".
  const end = offset + size;
  let out = "";
  for (let i = offset; i < end; i++) {
    const b = bytes[i] as number;
    if (b >= 0x80) {
      throw new BodyDecodeError(
        `non-ASCII byte 0x${b.toString(16)} at content offset ${i}`,
      );
    }
    out += String.fromCharCode(b);
  }
  return out;
}

function _writeAscii(
  bytes: Uint8Array,
  offset: number,
  size: number,
  value: string,
): void {
  if (value.length > size) {
    throw new BodyDecodeError(
      `string too long: ${value.length} chars > ${size} byte slot`,
    );
  }
  for (let i = 0; i < value.length; i++) {
    const c = value.charCodeAt(i);
    if (c >= 0x80) {
      throw new BodyDecodeError(
        `non-ASCII char 0x${c.toString(16)} at position ${i}; ` +
          `ASCII-only field`,
      );
    }
    bytes[offset + i] = c;
  }
  // Remaining bytes left as 0.
}

function _encodeAscii(value: string): Uint8Array {
  // Strict ASCII pack helper used by length_prefixed_string and
  // trailing_string fields with encoding="ascii". Rejects any
  // char >= 0x80 rather than silently truncating or emitting
  // encoded bytes — mirrors the Python codec's `value.encode("ascii")`.
  const out = new Uint8Array(value.length);
  for (let i = 0; i < value.length; i++) {
    const c = value.charCodeAt(i);
    if (c >= 0x80) {
      throw new BodyDecodeError(
        `non-ASCII char 0x${c.toString(16)} at position ${i}; ` +
          `ASCII-only field`,
      );
    }
    out[i] = c;
  }
  return out;
}


export interface HeaderMessageInit {
  version?: number;
  type?: string;
  device_name?: string;
  timestamp?: bigint;
  body_size?: bigint;
  crc?: bigint;
}

export class HeaderMessage {
  static readonly TYPE_ID = "HEADER";
  static readonly BODY_SIZE = 58;

  version: number;
  type: string;
  device_name: string;
  timestamp: bigint;
  body_size: bigint;
  crc: bigint;

  constructor(init: HeaderMessageInit = {}) {
    this.version = init.version ?? 0;
    this.type = init.type ?? "";
    this.device_name = init.device_name ?? "";
    this.timestamp = init.timestamp ?? 0n;
    this.body_size = init.body_size ?? 0n;
    this.crc = init.crc ?? 0n;
  }

  static unpack(bytes: Uint8Array): HeaderMessage {
    if (bytes.length !== 58) {
      throw new BodyDecodeError(
        `HEADER body must be 58 bytes, got ${bytes.length}`,
      );
    }
    const view = viewOf(bytes);
    let offset = 0;
    const version = readU16(view, offset); offset += 2;
    const type = _readAscii(bytes, offset, 12); offset += 12;
    const device_name = _readAscii(bytes, offset, 20); offset += 20;
    const timestamp = readU64(view, offset); offset += 8;
    const body_size = readU64(view, offset); offset += 8;
    const crc = readU64(view, offset); offset += 8;
    if (offset !== bytes.length) {
      throw new BodyDecodeError(
        `HEADER unpack consumed ${offset}/${bytes.length} bytes`,
      );
    }
    const instance = new HeaderMessage({
      version,
      type,
      device_name,
      timestamp,
      body_size,
      crc,
    });
    return instance;
  }

  pack(): Uint8Array {
    const parts: Uint8Array[] = [];
    {
      const part = new Uint8Array(2);
      writeU16(viewOf(part), 0, this.version);
      parts.push(part);
    }
    {
      const part = new Uint8Array(12);
      _writeAscii(part, 0, 12, this.type);
      parts.push(part);
    }
    {
      const part = new Uint8Array(20);
      _writeAscii(part, 0, 20, this.device_name);
      parts.push(part);
    }
    {
      const part = new Uint8Array(8);
      writeU64(viewOf(part), 0, this.timestamp);
      parts.push(part);
    }
    {
      const part = new Uint8Array(8);
      writeU64(viewOf(part), 0, this.body_size);
      parts.push(part);
    }
    {
      const part = new Uint8Array(8);
      writeU64(viewOf(part), 0, this.crc);
      parts.push(part);
    }
    let total = 0;
    for (const p of parts) total += p.length;
    const out = new Uint8Array(total);
    let o = 0;
    for (const p of parts) { out.set(p, o); o += p.length; }
    return out;
  }
}
