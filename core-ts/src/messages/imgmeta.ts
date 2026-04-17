// GENERATED from spec/schemas/imgmeta.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen ts
/**
 * IMGMETA — typed TypeScript wire codec.
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

export interface ImageEntry {
  name: string;
  device_name: string;
  modality: string;
  patient_name: string;
  patient_id: string;
  timestamp: bigint;
  size: number[];
  scalar_type: number;
  reserved: number;
}


export interface ImgmetaInit {
  images?: ImageEntry[];
}

export class Imgmeta {
  static readonly TYPE_ID = "IMGMETA";

  images: ImageEntry[];

  constructor(init: ImgmetaInit = {}) {
    this.images = init.images ?? [];
  }

  static unpack(bytes: Uint8Array): Imgmeta {
    const view = viewOf(bytes);
    let offset = 0;
    const _n_images = (bytes.length - offset) / 260;
    if (!Number.isInteger(_n_images)) throw new BodyDecodeError(`images: remaining bytes not divisible by element size 260`);
    const images: ImageEntry[] = new Array(_n_images);
    for (let _i = 0; _i < _n_images; _i++) {
    const sub_name = _readAscii(bytes, offset + 0, 64);
    const sub_device_name = _readAscii(bytes, offset + 64, 20);
    const sub_modality = _readAscii(bytes, offset + 84, 32);
    const sub_patient_name = _readAscii(bytes, offset + 116, 64);
    const sub_patient_id = _readAscii(bytes, offset + 180, 64);
    const sub_timestamp = readU64(view, offset + 244);
    const sub_size: number[] = new Array(3);
    for (let _i = 0; _i < 3; _i++) sub_size[_i] = readU16(view, offset + 252 + _i * 2);
    const sub_scalar_type = readU8(view, offset + 258);
    const sub_reserved = readU8(view, offset + 259);
      images[_i] = { name: sub_name, device_name: sub_device_name, modality: sub_modality, patient_name: sub_patient_name, patient_id: sub_patient_id, timestamp: sub_timestamp, size: sub_size, scalar_type: sub_scalar_type, reserved: sub_reserved };
      offset += 260;
    }
    if (offset !== bytes.length) {
      throw new BodyDecodeError(
        `IMGMETA unpack consumed ${offset}/${bytes.length} bytes`,
      );
    }
    const instance = new Imgmeta({
      images,
    });
    return instance;
  }

  pack(): Uint8Array {
    const parts: Uint8Array[] = [];
    {
      const part = new Uint8Array(this.images.length * 260);
      const pv = viewOf(part);
      let partOff = 0;
      const partBytes = part;
      const partView = pv;
      for (const x of this.images) {
    _writeAscii(partBytes, partOff + 0, 64, x.name);
    _writeAscii(partBytes, partOff + 64, 20, x.device_name);
    _writeAscii(partBytes, partOff + 84, 32, x.modality);
    _writeAscii(partBytes, partOff + 116, 64, x.patient_name);
    _writeAscii(partBytes, partOff + 180, 64, x.patient_id);
    writeU64(partView, partOff + 244, x.timestamp);
    for (let _i = 0; _i < 3; _i++) writeU16(partView, partOff + 252 + _i * 2, x.size[_i] as number);
    writeU8(partView, partOff + 258, x.scalar_type);
    writeU8(partView, partOff + 259, x.reserved);
        partOff += 260;
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
