// GENERATED from spec/schemas/capability.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen ts
/**
 * CAPABILITY — typed TypeScript wire codec.
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


export interface CapabilityInit {
  supported_types?: string[];
}

export class Capability {
  static readonly TYPE_ID = "CAPABILITY";

  supported_types: string[];

  constructor(init: CapabilityInit = {}) {
    this.supported_types = init.supported_types ?? [];
  }

  static unpack(bytes: Uint8Array): Capability {
    const view = viewOf(bytes);
    let offset = 0;
    const _n_supported_types = (bytes.length - offset) / 12;
    const supported_types: string[] = new Array(_n_supported_types);
    for (let _i = 0; _i < _n_supported_types; _i++) { supported_types[_i] = _readAscii(bytes, offset, 12); offset += 12; }
    if (offset !== bytes.length) {
      throw new BodyDecodeError(
        `CAPABILITY unpack consumed ${offset}/${bytes.length} bytes`,
      );
    }
    return new Capability({
      supported_types,
    });
  }

  pack(): Uint8Array {
    const parts: Uint8Array[] = [];
    {
      const part = new Uint8Array(this.supported_types.length * 12);
      for (let _i = 0; _i < this.supported_types.length; _i++) _writeAscii(part, _i * 12, 12, this.supported_types[_i] as string);
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
