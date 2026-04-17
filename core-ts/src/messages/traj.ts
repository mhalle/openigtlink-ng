// GENERATED from spec/schemas/traj.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen ts
/**
 * TRAJ — typed TypeScript wire codec.
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

export interface TrajectoryEntry {
  name: string;
  group_name: string;
  type: number;
  reserved: number;
  rgba: Uint8Array;
  entry_pos: number[];
  target_pos: number[];
  radius: number;
  owner_name: string;
}


export interface TrajInit {
  trajectories?: TrajectoryEntry[];
}

export class Traj {
  static readonly TYPE_ID = "TRAJ";

  trajectories: TrajectoryEntry[];

  constructor(init: TrajInit = {}) {
    this.trajectories = init.trajectories ?? [];
  }

  static unpack(bytes: Uint8Array): Traj {
    const view = viewOf(bytes);
    let offset = 0;
    const _n_trajectories = (bytes.length - offset) / 150;
    if (!Number.isInteger(_n_trajectories)) throw new BodyDecodeError(`trajectories: remaining bytes not divisible by element size 150`);
    const trajectories: TrajectoryEntry[] = new Array(_n_trajectories);
    for (let _i = 0; _i < _n_trajectories; _i++) {
    const sub_name = _readAscii(bytes, offset + 0, 64);
    const sub_group_name = _readAscii(bytes, offset + 64, 32);
    const sub_type = readI8(view, offset + 96);
    const sub_reserved = readI8(view, offset + 97);
    const sub_rgba = new Uint8Array(bytes.subarray(offset + 98, offset + 98 + 4));
    const sub_entry_pos: number[] = new Array(3);
    for (let _i = 0; _i < 3; _i++) sub_entry_pos[_i] = readF32(view, offset + 102 + _i * 4);
    const sub_target_pos: number[] = new Array(3);
    for (let _i = 0; _i < 3; _i++) sub_target_pos[_i] = readF32(view, offset + 114 + _i * 4);
    const sub_radius = readF32(view, offset + 126);
    const sub_owner_name = _readAscii(bytes, offset + 130, 20);
      trajectories[_i] = { name: sub_name, group_name: sub_group_name, type: sub_type, reserved: sub_reserved, rgba: sub_rgba, entry_pos: sub_entry_pos, target_pos: sub_target_pos, radius: sub_radius, owner_name: sub_owner_name };
      offset += 150;
    }
    if (offset !== bytes.length) {
      throw new BodyDecodeError(
        `TRAJ unpack consumed ${offset}/${bytes.length} bytes`,
      );
    }
    const instance = new Traj({
      trajectories,
    });
    return instance;
  }

  pack(): Uint8Array {
    const parts: Uint8Array[] = [];
    {
      const part = new Uint8Array(this.trajectories.length * 150);
      const pv = viewOf(part);
      let partOff = 0;
      const partBytes = part;
      const partView = pv;
      for (const x of this.trajectories) {
    _writeAscii(partBytes, partOff + 0, 64, x.name);
    _writeAscii(partBytes, partOff + 64, 32, x.group_name);
    writeI8(partView, partOff + 96, x.type);
    writeI8(partView, partOff + 97, x.reserved);
    partBytes.set(x.rgba, partOff + 98);
    for (let _i = 0; _i < 3; _i++) writeF32(partView, partOff + 102 + _i * 4, x.entry_pos[_i] as number);
    for (let _i = 0; _i < 3; _i++) writeF32(partView, partOff + 114 + _i * 4, x.target_pos[_i] as number);
    writeF32(partView, partOff + 126, x.radius);
    _writeAscii(partBytes, partOff + 130, 20, x.owner_name);
        partOff += 150;
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
