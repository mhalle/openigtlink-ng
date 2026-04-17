// GENERATED from spec/schemas/bind.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen ts
/**
 * BIND — typed TypeScript wire codec.
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

export interface HeaderEntryEntry {
  type_id: string;
  body_size: bigint;
}


export interface BindInit {
  ncmessages?: number;
  header_entries?: HeaderEntryEntry[];
  nametable_size?: number;
  name_table?: Uint8Array;
  bodies?: Uint8Array;
}

export class Bind {
  static readonly TYPE_ID = "BIND";

  ncmessages: number;
  header_entries: HeaderEntryEntry[];
  nametable_size: number;
  name_table: Uint8Array;
  bodies: Uint8Array;

  constructor(init: BindInit = {}) {
    this.ncmessages = init.ncmessages ?? 0;
    this.header_entries = init.header_entries ?? [];
    this.nametable_size = init.nametable_size ?? 0;
    this.name_table = init.name_table ?? new Uint8Array(0);
    this.bodies = init.bodies ?? new Uint8Array(0);
  }

  static unpack(bytes: Uint8Array): Bind {
    const view = viewOf(bytes);
    let offset = 0;
    const ncmessages = readU16(view, offset); offset += 2;
    const _n_header_entries = Number(ncmessages);
    const header_entries: HeaderEntryEntry[] = new Array(_n_header_entries);
    for (let _i = 0; _i < _n_header_entries; _i++) {
    const sub_type_id = _readAscii(bytes, offset + 0, 12);
    const sub_body_size = readU64(view, offset + 12);
      header_entries[_i] = { type_id: sub_type_id, body_size: sub_body_size };
      offset += 20;
    }
    const nametable_size = readU16(view, offset); offset += 2;
    const _n_name_table = Number(nametable_size);
    const name_table = new Uint8Array(bytes.subarray(offset, offset + _n_name_table)); offset += _n_name_table;
    const bodies = new Uint8Array(bytes.subarray(offset, bytes.length)); offset = bytes.length;
    if (offset !== bytes.length) {
      throw new BodyDecodeError(
        `BIND unpack consumed ${offset}/${bytes.length} bytes`,
      );
    }
    return new Bind({
      ncmessages,
      header_entries,
      nametable_size,
      name_table,
      bodies,
    });
  }

  pack(): Uint8Array {
    const parts: Uint8Array[] = [];
    {
      const part = new Uint8Array(2);
      writeU16(viewOf(part), 0, this.ncmessages);
      parts.push(part);
    }
    {
      const part = new Uint8Array(this.header_entries.length * 20);
      const pv = viewOf(part);
      let partOff = 0;
      const partBytes = part;
      const partView = pv;
      for (const x of this.header_entries) {
    _writeAscii(partBytes, partOff + 0, 12, x.type_id);
    writeU64(partView, partOff + 12, x.body_size);
        partOff += 20;
      }
      parts.push(part);
    }
    {
      const part = new Uint8Array(2);
      writeU16(viewOf(part), 0, this.nametable_size);
      parts.push(part);
    }
    parts.push(new Uint8Array(this.name_table));
    parts.push(new Uint8Array(this.bodies));
    let total = 0;
    for (const p of parts) total += p.length;
    const out = new Uint8Array(total);
    let o = 0;
    for (const p of parts) { out.set(p, o); o += p.length; }
    return out;
  }
}
