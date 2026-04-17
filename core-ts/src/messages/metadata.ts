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
    const instance = new Metadata({
      count,
      index_entries,
      body,
    });
    return instance;
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
