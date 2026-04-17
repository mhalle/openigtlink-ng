// GENERATED from spec/schemas/transform.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen ts
/**
 * TRANSFORM — typed TypeScript wire codec.
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


export interface TransformInit {
  matrix?: number[];
}

export class Transform {
  static readonly TYPE_ID = "TRANSFORM";
  static readonly BODY_SIZE = 48;

  matrix: number[];

  constructor(init: TransformInit = {}) {
    this.matrix = init.matrix ?? new Array<number>(12).fill(0);
  }

  static unpack(bytes: Uint8Array): Transform {
    if (bytes.length !== 48) {
      throw new BodyDecodeError(
        `TRANSFORM body must be 48 bytes, got ${bytes.length}`,
      );
    }
    const view = viewOf(bytes);
    let offset = 0;
    const matrix: number[] = new Array(12);
    for (let _i = 0; _i < 12; _i++) { matrix[_i] = readF32(view, offset); offset += 4; }
    if (offset !== bytes.length) {
      throw new BodyDecodeError(
        `TRANSFORM unpack consumed ${offset}/${bytes.length} bytes`,
      );
    }
    return new Transform({
      matrix,
    });
  }

  pack(): Uint8Array {
    const parts: Uint8Array[] = [];
    {
      const part = new Uint8Array(12 * 4);
      const pv = viewOf(part);
      for (let _i = 0; _i < 12; _i++) writeF32(pv, _i * 4, this.matrix[_i] as number);
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
