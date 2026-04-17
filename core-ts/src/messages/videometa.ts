// GENERATED from spec/schemas/videometa.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen ts
/**
 * VIDEOMETA — typed TypeScript wire codec.
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

export interface VideoEntry {
  name: string;
  device_name: string;
  patient_name: string;
  patient_id: string;
  zoom_level: number;
  focal_length: number;
  size: number[];
  matrix: number[];
  scalar_type: number;
  reserved: number;
}


export interface VideometaInit {
  videos?: VideoEntry[];
}

export class Videometa {
  static readonly TYPE_ID = "VIDEOMETA";

  videos: VideoEntry[];

  constructor(init: VideometaInit = {}) {
    this.videos = init.videos ?? [];
  }

  static unpack(bytes: Uint8Array): Videometa {
    const view = viewOf(bytes);
    let offset = 0;
    const _n_videos = (bytes.length - offset) / 322;
    if (!Number.isInteger(_n_videos)) throw new BodyDecodeError(`videos: remaining bytes not divisible by element size 322`);
    const videos: VideoEntry[] = new Array(_n_videos);
    for (let _i = 0; _i < _n_videos; _i++) {
    const sub_name = _readAscii(bytes, offset + 0, 64);
    const sub_device_name = _readAscii(bytes, offset + 64, 64);
    const sub_patient_name = _readAscii(bytes, offset + 128, 64);
    const sub_patient_id = _readAscii(bytes, offset + 192, 64);
    const sub_zoom_level = readI16(view, offset + 256);
    const sub_focal_length = readF64(view, offset + 258);
    const sub_size: number[] = new Array(3);
    for (let _i = 0; _i < 3; _i++) sub_size[_i] = readU16(view, offset + 266 + _i * 2);
    const sub_matrix: number[] = new Array(12);
    for (let _i = 0; _i < 12; _i++) sub_matrix[_i] = readF32(view, offset + 272 + _i * 4);
    const sub_scalar_type = readU8(view, offset + 320);
    const sub_reserved = readU8(view, offset + 321);
      videos[_i] = { name: sub_name, device_name: sub_device_name, patient_name: sub_patient_name, patient_id: sub_patient_id, zoom_level: sub_zoom_level, focal_length: sub_focal_length, size: sub_size, matrix: sub_matrix, scalar_type: sub_scalar_type, reserved: sub_reserved };
      offset += 322;
    }
    if (offset !== bytes.length) {
      throw new BodyDecodeError(
        `VIDEOMETA unpack consumed ${offset}/${bytes.length} bytes`,
      );
    }
    return new Videometa({
      videos,
    });
  }

  pack(): Uint8Array {
    const parts: Uint8Array[] = [];
    {
      const part = new Uint8Array(this.videos.length * 322);
      const pv = viewOf(part);
      let partOff = 0;
      const partBytes = part;
      const partView = pv;
      for (const x of this.videos) {
    _writeAscii(partBytes, partOff + 0, 64, x.name);
    _writeAscii(partBytes, partOff + 64, 64, x.device_name);
    _writeAscii(partBytes, partOff + 128, 64, x.patient_name);
    _writeAscii(partBytes, partOff + 192, 64, x.patient_id);
    writeI16(partView, partOff + 256, x.zoom_level);
    writeF64(partView, partOff + 258, x.focal_length);
    for (let _i = 0; _i < 3; _i++) writeU16(partView, partOff + 266 + _i * 2, x.size[_i] as number);
    for (let _i = 0; _i < 12; _i++) writeF32(partView, partOff + 272 + _i * 4, x.matrix[_i] as number);
    writeU8(partView, partOff + 320, x.scalar_type);
    writeU8(partView, partOff + 321, x.reserved);
        partOff += 322;
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
