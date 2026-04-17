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
    return new Traj({
      trajectories,
    });
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
