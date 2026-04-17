// GENERATED from spec/schemas/stt_bind.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen ts
/**
 * STT_BIND — typed TypeScript wire codec.
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


export interface SttBindInit {
  resolution?: bigint;
  ncmessages?: number;
  type_ids?: string[];
  nametable_size?: number;
  name_table?: Uint8Array;
}

export class SttBind {
  static readonly TYPE_ID = "STT_BIND";

  resolution: bigint;
  ncmessages: number;
  type_ids: string[];
  nametable_size: number;
  name_table: Uint8Array;

  constructor(init: SttBindInit = {}) {
    this.resolution = init.resolution ?? 0n;
    this.ncmessages = init.ncmessages ?? 0;
    this.type_ids = init.type_ids ?? [];
    this.nametable_size = init.nametable_size ?? 0;
    this.name_table = init.name_table ?? new Uint8Array(0);
  }

  static unpack(bytes: Uint8Array): SttBind {
    const view = viewOf(bytes);
    let offset = 0;
    const resolution = readU64(view, offset); offset += 8;
    const ncmessages = readU16(view, offset); offset += 2;
    const _n_type_ids = Number(ncmessages);
    const type_ids: string[] = new Array(_n_type_ids);
    for (let _i = 0; _i < _n_type_ids; _i++) { type_ids[_i] = _readAscii(bytes, offset, 12); offset += 12; }
    const nametable_size = readU16(view, offset); offset += 2;
    const _n_name_table = Number(nametable_size);
    const name_table = new Uint8Array(bytes.subarray(offset, offset + _n_name_table)); offset += _n_name_table;
    if (offset !== bytes.length) {
      throw new BodyDecodeError(
        `STT_BIND unpack consumed ${offset}/${bytes.length} bytes`,
      );
    }
    return new SttBind({
      resolution,
      ncmessages,
      type_ids,
      nametable_size,
      name_table,
    });
  }

  pack(): Uint8Array {
    const parts: Uint8Array[] = [];
    {
      const part = new Uint8Array(8);
      writeU64(viewOf(part), 0, this.resolution);
      parts.push(part);
    }
    {
      const part = new Uint8Array(2);
      writeU16(viewOf(part), 0, this.ncmessages);
      parts.push(part);
    }
    {
      const part = new Uint8Array(this.type_ids.length * 12);
      for (let _i = 0; _i < this.type_ids.length; _i++) _writeAscii(part, _i * 12, 12, this.type_ids[_i] as string);
      parts.push(part);
    }
    {
      const part = new Uint8Array(2);
      writeU16(viewOf(part), 0, this.nametable_size);
      parts.push(part);
    }
    parts.push(new Uint8Array(this.name_table));
    let total = 0;
    for (const p of parts) total += p.length;
    const out = new Uint8Array(total);
    let o = 0;
    for (const p of parts) { out.set(p, o); o += p.length; }
    return out;
  }
}
