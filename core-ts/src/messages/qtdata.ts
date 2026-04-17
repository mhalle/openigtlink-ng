// GENERATED from spec/schemas/qtdata.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen ts
/**
 * QTDATA — typed TypeScript wire codec.
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

export interface ToolEntry {
  name: string;
  type: number;
  reserved: number;
  position: number[];
  quaternion: number[];
}


export interface QtdataInit {
  tools?: ToolEntry[];
}

export class Qtdata {
  static readonly TYPE_ID = "QTDATA";

  tools: ToolEntry[];

  constructor(init: QtdataInit = {}) {
    this.tools = init.tools ?? [];
  }

  static unpack(bytes: Uint8Array): Qtdata {
    const view = viewOf(bytes);
    let offset = 0;
    const _n_tools = (bytes.length - offset) / 50;
    if (!Number.isInteger(_n_tools)) throw new BodyDecodeError(`tools: remaining bytes not divisible by element size 50`);
    const tools: ToolEntry[] = new Array(_n_tools);
    for (let _i = 0; _i < _n_tools; _i++) {
    const sub_name = _readAscii(bytes, offset + 0, 20);
    const sub_type = readU8(view, offset + 20);
    const sub_reserved = readU8(view, offset + 21);
    const sub_position: number[] = new Array(3);
    for (let _i = 0; _i < 3; _i++) sub_position[_i] = readF32(view, offset + 22 + _i * 4);
    const sub_quaternion: number[] = new Array(4);
    for (let _i = 0; _i < 4; _i++) sub_quaternion[_i] = readF32(view, offset + 34 + _i * 4);
      tools[_i] = { name: sub_name, type: sub_type, reserved: sub_reserved, position: sub_position, quaternion: sub_quaternion };
      offset += 50;
    }
    if (offset !== bytes.length) {
      throw new BodyDecodeError(
        `QTDATA unpack consumed ${offset}/${bytes.length} bytes`,
      );
    }
    return new Qtdata({
      tools,
    });
  }

  pack(): Uint8Array {
    const parts: Uint8Array[] = [];
    {
      const part = new Uint8Array(this.tools.length * 50);
      const pv = viewOf(part);
      let partOff = 0;
      const partBytes = part;
      const partView = pv;
      for (const x of this.tools) {
    _writeAscii(partBytes, partOff + 0, 20, x.name);
    writeU8(partView, partOff + 20, x.type);
    writeU8(partView, partOff + 21, x.reserved);
    for (let _i = 0; _i < 3; _i++) writeF32(partView, partOff + 22 + _i * 4, x.position[_i] as number);
    for (let _i = 0; _i < 4; _i++) writeF32(partView, partOff + 34 + _i * 4, x.quaternion[_i] as number);
        partOff += 50;
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
