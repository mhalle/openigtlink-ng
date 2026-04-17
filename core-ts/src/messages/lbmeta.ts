// GENERATED from spec/schemas/lbmeta.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen ts
/**
 * LBMETA — typed TypeScript wire codec.
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

export interface LabelEntry {
  name: string;
  device_name: string;
  label: number;
  reserved: number;
  rgba: Uint8Array;
  size: number[];
  owner: string;
}


export interface LbmetaInit {
  labels?: LabelEntry[];
}

export class Lbmeta {
  static readonly TYPE_ID = "LBMETA";

  labels: LabelEntry[];

  constructor(init: LbmetaInit = {}) {
    this.labels = init.labels ?? [];
  }

  static unpack(bytes: Uint8Array): Lbmeta {
    const view = viewOf(bytes);
    let offset = 0;
    const _n_labels = (bytes.length - offset) / 116;
    if (!Number.isInteger(_n_labels)) throw new BodyDecodeError(`labels: remaining bytes not divisible by element size 116`);
    const labels: LabelEntry[] = new Array(_n_labels);
    for (let _i = 0; _i < _n_labels; _i++) {
    const sub_name = _readAscii(bytes, offset + 0, 64);
    const sub_device_name = _readAscii(bytes, offset + 64, 20);
    const sub_label = readU8(view, offset + 84);
    const sub_reserved = readU8(view, offset + 85);
    const sub_rgba = new Uint8Array(bytes.subarray(offset + 86, offset + 86 + 4));
    const sub_size: number[] = new Array(3);
    for (let _i = 0; _i < 3; _i++) sub_size[_i] = readU16(view, offset + 90 + _i * 2);
    const sub_owner = _readAscii(bytes, offset + 96, 20);
      labels[_i] = { name: sub_name, device_name: sub_device_name, label: sub_label, reserved: sub_reserved, rgba: sub_rgba, size: sub_size, owner: sub_owner };
      offset += 116;
    }
    if (offset !== bytes.length) {
      throw new BodyDecodeError(
        `LBMETA unpack consumed ${offset}/${bytes.length} bytes`,
      );
    }
    return new Lbmeta({
      labels,
    });
  }

  pack(): Uint8Array {
    const parts: Uint8Array[] = [];
    {
      const part = new Uint8Array(this.labels.length * 116);
      const pv = viewOf(part);
      let partOff = 0;
      const partBytes = part;
      const partView = pv;
      for (const x of this.labels) {
    _writeAscii(partBytes, partOff + 0, 64, x.name);
    _writeAscii(partBytes, partOff + 64, 20, x.device_name);
    writeU8(partView, partOff + 84, x.label);
    writeU8(partView, partOff + 85, x.reserved);
    partBytes.set(x.rgba, partOff + 86);
    for (let _i = 0; _i < 3; _i++) writeU16(partView, partOff + 90 + _i * 2, x.size[_i] as number);
    _writeAscii(partBytes, partOff + 96, 20, x.owner);
        partOff += 116;
      }
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
