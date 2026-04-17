// GENERATED from spec/schemas/metadata.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen ts
/**
 * METADATA — typed TypeScript wire codec.
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

export interface IndexEntryEntry {
  key_size: number;
  value_encoding: number;
  value_size: number;
}


export interface MetadataInit {
  count?: number;
  index_entries?: IndexEntryEntry[];
  body?: Uint8Array;
}

export class Metadata {
  static readonly TYPE_ID = "METADATA";

  count: number;
  index_entries: IndexEntryEntry[];
  body: Uint8Array;

  constructor(init: MetadataInit = {}) {
    this.count = init.count ?? 0;
    this.index_entries = init.index_entries ?? [];
    this.body = init.body ?? new Uint8Array(0);
  }

  static unpack(bytes: Uint8Array): Metadata {
    const view = viewOf(bytes);
    let offset = 0;
    const count = readU16(view, offset); offset += 2;
    const _n_index_entries = Number(count);
    const index_entries: IndexEntryEntry[] = new Array(_n_index_entries);
    for (let _i = 0; _i < _n_index_entries; _i++) {
    const sub_key_size = readU16(view, offset + 0);
    const sub_value_encoding = readU16(view, offset + 2);
    const sub_value_size = readU32(view, offset + 4);
      index_entries[_i] = { key_size: sub_key_size, value_encoding: sub_value_encoding, value_size: sub_value_size };
      offset += 8;
    }
    const body = new Uint8Array(bytes.subarray(offset, bytes.length)); offset = bytes.length;
    if (offset !== bytes.length) {
      throw new BodyDecodeError(
        `METADATA unpack consumed ${offset}/${bytes.length} bytes`,
      );
    }
    return new Metadata({
      count,
      index_entries,
      body,
    });
  }

  pack(): Uint8Array {
    const parts: Uint8Array[] = [];
    {
      const part = new Uint8Array(2);
      writeU16(viewOf(part), 0, this.count);
      parts.push(part);
    }
    {
      const part = new Uint8Array(this.index_entries.length * 8);
      const pv = viewOf(part);
      let partOff = 0;
      const partBytes = part;
      const partView = pv;
      for (const x of this.index_entries) {
    writeU16(partView, partOff + 0, x.key_size);
    writeU16(partView, partOff + 2, x.value_encoding);
    writeU32(partView, partOff + 4, x.value_size);
        partOff += 8;
      }
      parts.push(part);
    }
    parts.push(new Uint8Array(this.body));
    let total = 0;
    for (const p of parts) total += p.length;
    const out = new Uint8Array(total);
    let o = 0;
    for (const p of parts) { out.set(p, o); o += p.length; }
    return out;
  }
}
