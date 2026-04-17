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

// Minimal ASCII helpers used by fixed_string / struct-array sub-fields.
// Split out so the generated classes can stay import-light.
function _readAscii(bytes: Uint8Array, offset: number, size: number): string {
  let end = offset + size;
  while (end > offset && bytes[end - 1] === 0) end--;
  let out = "";
  for (let i = offset; i < end; i++) out += String.fromCharCode(bytes[i] as number);
  return out;
}

function _readAsciiRaw(bytes: Uint8Array, offset: number, size: number): string {
  let out = "";
  for (let i = offset; i < offset + size; i++) out += String.fromCharCode(bytes[i] as number);
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
    bytes[offset + i] = value.charCodeAt(i) & 0xff;
  }
  // Remaining bytes left as 0.
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
    return new HeaderMessage({
      version,
      type,
      device_name,
      timestamp,
      body_size,
      crc,
    });
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
