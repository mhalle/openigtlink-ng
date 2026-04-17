// GENERATED from spec/schemas/point.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen ts
/**
 * POINT — typed TypeScript wire codec.
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

export interface PointEntry {
  name: string;
  group_name: string;
  rgba: Uint8Array;
  position: number[];
  radius: number;
  owner: string;
}


export interface PointInit {
  points?: PointEntry[];
}

export class Point {
  static readonly TYPE_ID = "POINT";

  points: PointEntry[];

  constructor(init: PointInit = {}) {
    this.points = init.points ?? [];
  }

  static unpack(bytes: Uint8Array): Point {
    const view = viewOf(bytes);
    let offset = 0;
    const _n_points = (bytes.length - offset) / 136;
    if (!Number.isInteger(_n_points)) throw new BodyDecodeError(`points: remaining bytes not divisible by element size 136`);
    const points: PointEntry[] = new Array(_n_points);
    for (let _i = 0; _i < _n_points; _i++) {
    const sub_name = _readAscii(bytes, offset + 0, 64);
    const sub_group_name = _readAscii(bytes, offset + 64, 32);
    const sub_rgba = new Uint8Array(bytes.subarray(offset + 96, offset + 96 + 4));
    const sub_position: number[] = new Array(3);
    for (let _i = 0; _i < 3; _i++) sub_position[_i] = readF32(view, offset + 100 + _i * 4);
    const sub_radius = readF32(view, offset + 112);
    const sub_owner = _readAscii(bytes, offset + 116, 20);
      points[_i] = { name: sub_name, group_name: sub_group_name, rgba: sub_rgba, position: sub_position, radius: sub_radius, owner: sub_owner };
      offset += 136;
    }
    if (offset !== bytes.length) {
      throw new BodyDecodeError(
        `POINT unpack consumed ${offset}/${bytes.length} bytes`,
      );
    }
    return new Point({
      points,
    });
  }

  pack(): Uint8Array {
    const parts: Uint8Array[] = [];
    {
      const part = new Uint8Array(this.points.length * 136);
      const pv = viewOf(part);
      let partOff = 0;
      const partBytes = part;
      const partView = pv;
      for (const x of this.points) {
    _writeAscii(partBytes, partOff + 0, 64, x.name);
    _writeAscii(partBytes, partOff + 64, 32, x.group_name);
    partBytes.set(x.rgba, partOff + 96);
    for (let _i = 0; _i < 3; _i++) writeF32(partView, partOff + 100 + _i * 4, x.position[_i] as number);
    writeF32(partView, partOff + 112, x.radius);
    _writeAscii(partBytes, partOff + 116, 20, x.owner);
        partOff += 136;
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
