// GENERATED from spec/schemas/colortable.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen ts
/**
 * COLORTABLE — typed TypeScript wire codec.
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


export interface ColortableInit {
  index_type?: number;
  map_type?: number;
  table?: Uint8Array;
}

export class Colortable {
  static readonly TYPE_ID = "COLORTABLE";

  index_type: number;
  map_type: number;
  table: Uint8Array;

  constructor(init: ColortableInit = {}) {
    this.index_type = init.index_type ?? 0;
    this.map_type = init.map_type ?? 0;
    this.table = init.table ?? new Uint8Array(0);
  }

  static unpack(bytes: Uint8Array): Colortable {
    const view = viewOf(bytes);
    let offset = 0;
    const index_type = readI8(view, offset); offset += 1;
    const map_type = readI8(view, offset); offset += 1;
    const table = new Uint8Array(bytes.subarray(offset, bytes.length)); offset = bytes.length;
    if (offset !== bytes.length) {
      throw new BodyDecodeError(
        `COLORTABLE unpack consumed ${offset}/${bytes.length} bytes`,
      );
    }
    return new Colortable({
      index_type,
      map_type,
      table,
    });
  }

  pack(): Uint8Array {
    const parts: Uint8Array[] = [];
    {
      const part = new Uint8Array(1);
      writeI8(viewOf(part), 0, this.index_type);
      parts.push(part);
    }
    {
      const part = new Uint8Array(1);
      writeI8(viewOf(part), 0, this.map_type);
      parts.push(part);
    }
    parts.push(new Uint8Array(this.table));
    let total = 0;
    for (const p of parts) total += p.length;
    const out = new Uint8Array(total);
    let o = 0;
    for (const p of parts) { out.set(p, o); o += p.length; }
    return out;
  }
}
