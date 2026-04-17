// GENERATED from spec/schemas/string.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen ts
/**
 * STRING — typed TypeScript wire codec.
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


export interface StringMessageInit {
  encoding?: number;
  value?: Uint8Array;
}

export class StringMessage {
  static readonly TYPE_ID = "STRING";

  encoding: number;
  value: Uint8Array;

  constructor(init: StringMessageInit = {}) {
    this.encoding = init.encoding ?? 0;
    this.value = init.value ?? new Uint8Array(0);
  }

  static unpack(bytes: Uint8Array): StringMessage {
    const view = viewOf(bytes);
    let offset = 0;
    const encoding = readU16(view, offset); offset += 2;
    const _len = Number(readU16(view, offset)); offset += 2;
    const value = new Uint8Array(bytes.subarray(offset, offset + _len)); offset += _len;
    if (offset !== bytes.length) {
      throw new BodyDecodeError(
        `STRING unpack consumed ${offset}/${bytes.length} bytes`,
      );
    }
    return new StringMessage({
      encoding,
      value,
    });
  }

  pack(): Uint8Array {
    const parts: Uint8Array[] = [];
    {
      const part = new Uint8Array(2);
      writeU16(viewOf(part), 0, this.encoding);
      parts.push(part);
    }
    {
      const _len = this.value.length;
      const part = new Uint8Array(2 + _len);
      writeU16(viewOf(part), 0, _len);
      part.set(this.value, 2);
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
