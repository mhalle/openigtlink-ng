// GENERATED from spec/schemas/stt_tdata.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen ts
/**
 * STT_TDATA — typed TypeScript wire codec.
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


export interface SttTdataInit {
  resolution?: number;
  coord_name?: string;
}

export class SttTdata {
  static readonly TYPE_ID = "STT_TDATA";
  static readonly BODY_SIZE = 36;

  resolution: number;
  coord_name: string;

  constructor(init: SttTdataInit = {}) {
    this.resolution = init.resolution ?? 0;
    this.coord_name = init.coord_name ?? "";
  }

  static unpack(bytes: Uint8Array): SttTdata {
    if (bytes.length !== 36) {
      throw new BodyDecodeError(
        `STT_TDATA body must be 36 bytes, got ${bytes.length}`,
      );
    }
    const view = viewOf(bytes);
    let offset = 0;
    const resolution = readI32(view, offset); offset += 4;
    const coord_name = _readAscii(bytes, offset, 32); offset += 32;
    if (offset !== bytes.length) {
      throw new BodyDecodeError(
        `STT_TDATA unpack consumed ${offset}/${bytes.length} bytes`,
      );
    }
    return new SttTdata({
      resolution,
      coord_name,
    });
  }

  pack(): Uint8Array {
    const parts: Uint8Array[] = [];
    {
      const part = new Uint8Array(4);
      writeI32(viewOf(part), 0, this.resolution);
      parts.push(part);
    }
    {
      const part = new Uint8Array(32);
      _writeAscii(part, 0, 32, this.coord_name);
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
