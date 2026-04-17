// GENERATED from spec/schemas/ndarray.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen ts
/**
 * NDARRAY — typed TypeScript wire codec.
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


export interface NdarrayInit {
  scalar_type?: number;
  dim?: number;
  size?: Uint16Array;
  data?: Uint8Array;
}

export class Ndarray {
  static readonly TYPE_ID = "NDARRAY";

  scalar_type: number;
  dim: number;
  size: Uint16Array;
  data: Uint8Array;

  constructor(init: NdarrayInit = {}) {
    this.scalar_type = init.scalar_type ?? 0;
    this.dim = init.dim ?? 0;
    this.size = init.size ?? new Uint16Array(0);
    this.data = init.data ?? new Uint8Array(0);
  }

  static unpack(bytes: Uint8Array): Ndarray {
    const view = viewOf(bytes);
    let offset = 0;
    const scalar_type = readU8(view, offset); offset += 1;
    const dim = readU8(view, offset); offset += 1;
    const _n_size = Number(dim);
    const size = readU16ArrayBE(view, offset, _n_size); offset += _n_size * 2;
    const data = new Uint8Array(bytes.subarray(offset, bytes.length)); offset = bytes.length;
    if (offset !== bytes.length) {
      throw new BodyDecodeError(
        `NDARRAY unpack consumed ${offset}/${bytes.length} bytes`,
      );
    }
    return new Ndarray({
      scalar_type,
      dim,
      size,
      data,
    });
  }

  pack(): Uint8Array {
    const parts: Uint8Array[] = [];
    {
      const part = new Uint8Array(1);
      writeU8(viewOf(part), 0, this.scalar_type);
      parts.push(part);
    }
    {
      const part = new Uint8Array(1);
      writeU8(viewOf(part), 0, this.dim);
      parts.push(part);
    }
    {
      const part = new Uint8Array(this.size.length * 2);
      writeU16ArrayBE(viewOf(part), 0, this.size);
      parts.push(part);
    }
    parts.push(new Uint8Array(this.data));
    let total = 0;
    for (const p of parts) total += p.length;
    const out = new Uint8Array(total);
    let o = 0;
    for (const p of parts) { out.set(p, o); o += p.length; }
    return out;
  }
}
