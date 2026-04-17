// GENERATED from spec/schemas/polydata.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen ts
/**
 * POLYDATA — typed TypeScript wire codec.
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
  x: number;
  y: number;
  z: number;
}

export interface AttributeHeaderEntry {
  type: number;
  ncomponents: number;
  n: number;
}


export interface PolydataInit {
  npoints?: number;
  nvertices?: number;
  size_vertices?: number;
  nlines?: number;
  size_lines?: number;
  npolygons?: number;
  size_polygons?: number;
  ntriangle_strips?: number;
  size_triangle_strips?: number;
  nattributes?: number;
  points?: PointEntry[];
  vertices?: Uint8Array;
  lines?: Uint8Array;
  polygons?: Uint8Array;
  triangle_strips?: Uint8Array;
  attribute_headers?: AttributeHeaderEntry[];
  attribute_data?: Uint8Array;
}

export class Polydata {
  static readonly TYPE_ID = "POLYDATA";

  npoints: number;
  nvertices: number;
  size_vertices: number;
  nlines: number;
  size_lines: number;
  npolygons: number;
  size_polygons: number;
  ntriangle_strips: number;
  size_triangle_strips: number;
  nattributes: number;
  points: PointEntry[];
  vertices: Uint8Array;
  lines: Uint8Array;
  polygons: Uint8Array;
  triangle_strips: Uint8Array;
  attribute_headers: AttributeHeaderEntry[];
  attribute_data: Uint8Array;

  constructor(init: PolydataInit = {}) {
    this.npoints = init.npoints ?? 0;
    this.nvertices = init.nvertices ?? 0;
    this.size_vertices = init.size_vertices ?? 0;
    this.nlines = init.nlines ?? 0;
    this.size_lines = init.size_lines ?? 0;
    this.npolygons = init.npolygons ?? 0;
    this.size_polygons = init.size_polygons ?? 0;
    this.ntriangle_strips = init.ntriangle_strips ?? 0;
    this.size_triangle_strips = init.size_triangle_strips ?? 0;
    this.nattributes = init.nattributes ?? 0;
    this.points = init.points ?? [];
    this.vertices = init.vertices ?? new Uint8Array(0);
    this.lines = init.lines ?? new Uint8Array(0);
    this.polygons = init.polygons ?? new Uint8Array(0);
    this.triangle_strips = init.triangle_strips ?? new Uint8Array(0);
    this.attribute_headers = init.attribute_headers ?? [];
    this.attribute_data = init.attribute_data ?? new Uint8Array(0);
  }

  static unpack(bytes: Uint8Array): Polydata {
    const view = viewOf(bytes);
    let offset = 0;
    const npoints = readU32(view, offset); offset += 4;
    const nvertices = readU32(view, offset); offset += 4;
    const size_vertices = readU32(view, offset); offset += 4;
    const nlines = readU32(view, offset); offset += 4;
    const size_lines = readU32(view, offset); offset += 4;
    const npolygons = readU32(view, offset); offset += 4;
    const size_polygons = readU32(view, offset); offset += 4;
    const ntriangle_strips = readU32(view, offset); offset += 4;
    const size_triangle_strips = readU32(view, offset); offset += 4;
    const nattributes = readU32(view, offset); offset += 4;
    const _n_points = Number(npoints);
    const points: PointEntry[] = new Array(_n_points);
    for (let _i = 0; _i < _n_points; _i++) {
    const sub_x = readF32(view, offset + 0);
    const sub_y = readF32(view, offset + 4);
    const sub_z = readF32(view, offset + 8);
      points[_i] = { x: sub_x, y: sub_y, z: sub_z };
      offset += 12;
    }
    const _n_vertices = Number(size_vertices);
    const vertices = new Uint8Array(bytes.subarray(offset, offset + _n_vertices)); offset += _n_vertices;
    const _n_lines = Number(size_lines);
    const lines = new Uint8Array(bytes.subarray(offset, offset + _n_lines)); offset += _n_lines;
    const _n_polygons = Number(size_polygons);
    const polygons = new Uint8Array(bytes.subarray(offset, offset + _n_polygons)); offset += _n_polygons;
    const _n_triangle_strips = Number(size_triangle_strips);
    const triangle_strips = new Uint8Array(bytes.subarray(offset, offset + _n_triangle_strips)); offset += _n_triangle_strips;
    const _n_attribute_headers = Number(nattributes);
    const attribute_headers: AttributeHeaderEntry[] = new Array(_n_attribute_headers);
    for (let _i = 0; _i < _n_attribute_headers; _i++) {
    const sub_type = readU8(view, offset + 0);
    const sub_ncomponents = readU8(view, offset + 1);
    const sub_n = readU32(view, offset + 2);
      attribute_headers[_i] = { type: sub_type, ncomponents: sub_ncomponents, n: sub_n };
      offset += 6;
    }
    const attribute_data = new Uint8Array(bytes.subarray(offset, bytes.length)); offset = bytes.length;
    if (offset !== bytes.length) {
      throw new BodyDecodeError(
        `POLYDATA unpack consumed ${offset}/${bytes.length} bytes`,
      );
    }
    return new Polydata({
      npoints,
      nvertices,
      size_vertices,
      nlines,
      size_lines,
      npolygons,
      size_polygons,
      ntriangle_strips,
      size_triangle_strips,
      nattributes,
      points,
      vertices,
      lines,
      polygons,
      triangle_strips,
      attribute_headers,
      attribute_data,
    });
  }

  pack(): Uint8Array {
    const parts: Uint8Array[] = [];
    {
      const part = new Uint8Array(4);
      writeU32(viewOf(part), 0, this.npoints);
      parts.push(part);
    }
    {
      const part = new Uint8Array(4);
      writeU32(viewOf(part), 0, this.nvertices);
      parts.push(part);
    }
    {
      const part = new Uint8Array(4);
      writeU32(viewOf(part), 0, this.size_vertices);
      parts.push(part);
    }
    {
      const part = new Uint8Array(4);
      writeU32(viewOf(part), 0, this.nlines);
      parts.push(part);
    }
    {
      const part = new Uint8Array(4);
      writeU32(viewOf(part), 0, this.size_lines);
      parts.push(part);
    }
    {
      const part = new Uint8Array(4);
      writeU32(viewOf(part), 0, this.npolygons);
      parts.push(part);
    }
    {
      const part = new Uint8Array(4);
      writeU32(viewOf(part), 0, this.size_polygons);
      parts.push(part);
    }
    {
      const part = new Uint8Array(4);
      writeU32(viewOf(part), 0, this.ntriangle_strips);
      parts.push(part);
    }
    {
      const part = new Uint8Array(4);
      writeU32(viewOf(part), 0, this.size_triangle_strips);
      parts.push(part);
    }
    {
      const part = new Uint8Array(4);
      writeU32(viewOf(part), 0, this.nattributes);
      parts.push(part);
    }
    {
      const part = new Uint8Array(this.points.length * 12);
      const pv = viewOf(part);
      let partOff = 0;
      const partBytes = part;
      const partView = pv;
      for (const x of this.points) {
    writeF32(partView, partOff + 0, x.x);
    writeF32(partView, partOff + 4, x.y);
    writeF32(partView, partOff + 8, x.z);
        partOff += 12;
      }
      parts.push(part);
    }
    parts.push(new Uint8Array(this.vertices));
    parts.push(new Uint8Array(this.lines));
    parts.push(new Uint8Array(this.polygons));
    parts.push(new Uint8Array(this.triangle_strips));
    {
      const part = new Uint8Array(this.attribute_headers.length * 6);
      const pv = viewOf(part);
      let partOff = 0;
      const partBytes = part;
      const partView = pv;
      for (const x of this.attribute_headers) {
    writeU8(partView, partOff + 0, x.type);
    writeU8(partView, partOff + 1, x.ncomponents);
    writeU32(partView, partOff + 2, x.n);
        partOff += 6;
      }
      parts.push(part);
    }
    parts.push(new Uint8Array(this.attribute_data));
    let total = 0;
    for (const p of parts) total += p.length;
    const out = new Uint8Array(total);
    let o = 0;
    for (const p of parts) { out.set(p, o); o += p.length; }
    return out;
  }
}
