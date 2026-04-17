// GENERATED from spec/schemas/video.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen ts
/**
 * VIDEO — typed TypeScript wire codec.
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


export interface VideoMessageInit {
  header_version?: number;
  endian?: number;
  codec?: string;
  frame_type?: number;
  coord?: number;
  size?: number[];
  matrix?: number[];
  subvol_offset?: number[];
  subvol_size?: number[];
  frame_data?: Uint8Array;
}

export class VideoMessage {
  static readonly TYPE_ID = "VIDEO";

  header_version: number;
  endian: number;
  codec: string;
  frame_type: number;
  coord: number;
  size: number[];
  matrix: number[];
  subvol_offset: number[];
  subvol_size: number[];
  frame_data: Uint8Array;

  constructor(init: VideoMessageInit = {}) {
    this.header_version = init.header_version ?? 0;
    this.endian = init.endian ?? 0;
    this.codec = init.codec ?? "";
    this.frame_type = init.frame_type ?? 0;
    this.coord = init.coord ?? 0;
    this.size = init.size ?? new Array<number>(3).fill(0);
    this.matrix = init.matrix ?? new Array<number>(12).fill(0);
    this.subvol_offset = init.subvol_offset ?? new Array<number>(3).fill(0);
    this.subvol_size = init.subvol_size ?? new Array<number>(3).fill(0);
    this.frame_data = init.frame_data ?? new Uint8Array(0);
  }

  static unpack(bytes: Uint8Array): VideoMessage {
    const view = viewOf(bytes);
    let offset = 0;
    const header_version = readU16(view, offset); offset += 2;
    const endian = readU8(view, offset); offset += 1;
    const codec = _readAsciiRaw(bytes, offset, 4); offset += 4;
    const frame_type = readU16(view, offset); offset += 2;
    const coord = readU8(view, offset); offset += 1;
    const size: number[] = new Array(3);
    for (let _i = 0; _i < 3; _i++) { size[_i] = readU16(view, offset); offset += 2; }
    const matrix: number[] = new Array(12);
    for (let _i = 0; _i < 12; _i++) { matrix[_i] = readF32(view, offset); offset += 4; }
    const subvol_offset: number[] = new Array(3);
    for (let _i = 0; _i < 3; _i++) { subvol_offset[_i] = readU16(view, offset); offset += 2; }
    const subvol_size: number[] = new Array(3);
    for (let _i = 0; _i < 3; _i++) { subvol_size[_i] = readU16(view, offset); offset += 2; }
    const frame_data = new Uint8Array(bytes.subarray(offset, bytes.length)); offset = bytes.length;
    if (offset !== bytes.length) {
      throw new BodyDecodeError(
        `VIDEO unpack consumed ${offset}/${bytes.length} bytes`,
      );
    }
    return new VideoMessage({
      header_version,
      endian,
      codec,
      frame_type,
      coord,
      size,
      matrix,
      subvol_offset,
      subvol_size,
      frame_data,
    });
  }

  pack(): Uint8Array {
    const parts: Uint8Array[] = [];
    {
      const part = new Uint8Array(2);
      writeU16(viewOf(part), 0, this.header_version);
      parts.push(part);
    }
    {
      const part = new Uint8Array(1);
      writeU8(viewOf(part), 0, this.endian);
      parts.push(part);
    }
    {
      const part = new Uint8Array(4);
      _writeAscii(part, 0, 4, this.codec);
      parts.push(part);
    }
    {
      const part = new Uint8Array(2);
      writeU16(viewOf(part), 0, this.frame_type);
      parts.push(part);
    }
    {
      const part = new Uint8Array(1);
      writeU8(viewOf(part), 0, this.coord);
      parts.push(part);
    }
    {
      const part = new Uint8Array(3 * 2);
      const pv = viewOf(part);
      for (let _i = 0; _i < 3; _i++) writeU16(pv, _i * 2, this.size[_i] as number);
      parts.push(part);
    }
    {
      const part = new Uint8Array(12 * 4);
      const pv = viewOf(part);
      for (let _i = 0; _i < 12; _i++) writeF32(pv, _i * 4, this.matrix[_i] as number);
      parts.push(part);
    }
    {
      const part = new Uint8Array(3 * 2);
      const pv = viewOf(part);
      for (let _i = 0; _i < 3; _i++) writeU16(pv, _i * 2, this.subvol_offset[_i] as number);
      parts.push(part);
    }
    {
      const part = new Uint8Array(3 * 2);
      const pv = viewOf(part);
      for (let _i = 0; _i < 3; _i++) writeU16(pv, _i * 2, this.subvol_size[_i] as number);
      parts.push(part);
    }
    parts.push(new Uint8Array(this.frame_data));
    let total = 0;
    for (const p of parts) total += p.length;
    const out = new Uint8Array(total);
    let o = 0;
    for (const p of parts) { out.set(p, o); o += p.length; }
    return out;
  }
}
