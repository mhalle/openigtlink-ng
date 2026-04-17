/**
 * Cross-field post-unpack invariants.
 *
 * For a handful of message types, the spec defines constraints that
 * span multiple fields — e.g. NDARRAY requires
 * `len(data) === product(size) × bytesPerScalar(scalar_type)`.
 * These cannot be expressed per-field, so schemas name them via
 * `post_unpack_invariant` and every codec runtime implements the
 * same validator by name.
 *
 * Canonical reference:
 * corpus-tools/.../codec/policy.py::POST_UNPACK_INVARIANTS.
 * Python / C++ / TS all mirror it; the differential fuzzer holds
 * the four implementations in sync. Adding a new invariant requires
 * touching all four.
 *
 * Generated per-message `unpack` functions dispatch here via the
 * POST_UNPACK_INVARIANTS registry — see ts_message.ts.jinja.
 */

import { BodyDecodeError } from "./errors.js";

// NDARRAY scalar codes include complex (13) on top of the IMAGE set.
const NDARRAY_BYTES_PER_SCALAR = new Map<number, number>([
  [2, 1], [3, 1], [4, 2], [5, 2],
  [6, 4], [7, 4], [10, 4], [11, 8], [13, 16],
]);

const IMAGE_BYTES_PER_SCALAR = new Map<number, number>([
  [2, 1], [3, 1], [4, 2], [5, 2],
  [6, 4], [7, 4], [10, 4], [11, 8],
]);

function productU64(seq: ArrayLike<number>): bigint {
  // BigInt arithmetic matches the Python/C++ semantics: product
  // of up to 255 uint16 values can exceed 2^53; in practice our
  // guards catch it earlier, but we compute in BigInt to be safe.
  let p = 1n;
  for (let i = 0; i < seq.length; i++) {
    p *= BigInt(seq[i] as number);
  }
  return p;
}

function toNumberArray(value: unknown): number[] {
  if (ArrayBuffer.isView(value)) {
    // TypedArray (Uint8Array / Uint16Array / ...) — already numeric.
    const arr = value as unknown as ArrayLike<number>;
    const out: number[] = [];
    for (let i = 0; i < arr.length; i++) out.push(arr[i] as number);
    return out;
  }
  if (Array.isArray(value)) {
    return value.map((v) => Number(v));
  }
  throw new BodyDecodeError(
    `invariant helper: expected array-like, got ${typeof value}`,
  );
}

// NDARRAY: scalar_type ∈ {2,3,4,5,6,7,10,11,13} and
// len(data) === product(size) × bytesPerScalar(scalar_type).
export function checkNdarray(msg: {
  scalar_type: number;
  size: unknown;
  data: Uint8Array;
}): void {
  const bps = NDARRAY_BYTES_PER_SCALAR.get(msg.scalar_type);
  if (bps === undefined) {
    throw new BodyDecodeError(
      `NDARRAY: invalid scalar_type=${msg.scalar_type}`,
    );
  }
  const size = toNumberArray(msg.size);
  const prod = productU64(size);
  const expected = prod * BigInt(bps);
  if (BigInt(msg.data.length) !== expected) {
    throw new BodyDecodeError(
      `NDARRAY: data length ${msg.data.length} does not match ` +
        `product(size)=${prod} × bytes_per_scalar(${msg.scalar_type})=${bps} ` +
        `= ${expected}`,
    );
  }
}

// IMAGE: scalar_type ∈ {2,3,4,5,6,7,10,11}, endian ∈ {1,2,3},
// coord ∈ {1,2}, subvol_offset[i]+subvol_size[i] ≤ size[i],
// len(pixels) === product(subvol_size) × num_components × bytesPerScalar.
export function checkImage(msg: {
  scalar_type: number;
  endian: number;
  coord: number;
  num_components: number;
  size: unknown;
  subvol_offset: unknown;
  subvol_size: unknown;
  pixels: Uint8Array;
}): void {
  const bps = IMAGE_BYTES_PER_SCALAR.get(msg.scalar_type);
  if (bps === undefined) {
    throw new BodyDecodeError(
      `IMAGE: invalid scalar_type=${msg.scalar_type}`,
    );
  }
  if (msg.endian < 1 || msg.endian > 3) {
    throw new BodyDecodeError(`IMAGE: invalid endian=${msg.endian}`);
  }
  if (msg.coord < 1 || msg.coord > 2) {
    throw new BodyDecodeError(`IMAGE: invalid coord=${msg.coord}`);
  }
  const size = toNumberArray(msg.size);
  const subOff = toNumberArray(msg.subvol_offset);
  const subSize = toNumberArray(msg.subvol_size);
  for (let i = 0; i < size.length; i++) {
    if ((subOff[i] as number) + (subSize[i] as number) > (size[i] as number)) {
      throw new BodyDecodeError(
        `IMAGE: subvol_offset[${i}]+subvol_size[${i}]=${subOff[i]}+${subSize[i]} ` +
          `exceeds size[${i}]=${size[i]}`,
      );
    }
  }
  const prod = productU64(subSize);
  const expected =
    prod * BigInt(msg.num_components) * BigInt(bps);
  if (BigInt(msg.pixels.length) !== expected) {
    throw new BodyDecodeError(
      `IMAGE: pixels length ${msg.pixels.length} does not match ` +
        `product(subvol_size)=${prod} × num_components=${msg.num_components} ` +
        `× bytes_per_scalar(${msg.scalar_type})=${bps} = ${expected}`,
    );
  }
}

// COLORTABLE / COLORT:
//   index_type ∈ {3=uint8→256, 5=uint16→65536} entries
//   map_type   ∈ {3=1, 5=2, 19=3}               bytes/entry
// len(table) === entries × bytes_per_entry.
export function checkColortable(msg: {
  index_type: number;
  map_type: number;
  table: Uint8Array;
}): void {
  const entries = new Map<number, number>([[3, 256], [5, 65536]]);
  const bps = new Map<number, number>([[3, 1], [5, 2], [19, 3]]);
  const N = entries.get(msg.index_type);
  if (N === undefined) {
    throw new BodyDecodeError(
      `COLORT: invalid index_type=${msg.index_type}`,
    );
  }
  const S = bps.get(msg.map_type);
  if (S === undefined) {
    throw new BodyDecodeError(
      `COLORT: invalid map_type=${msg.map_type}`,
    );
  }
  const expected = N * S;
  if (msg.table.length !== expected) {
    throw new BodyDecodeError(
      `COLORT: table length ${msg.table.length} does not match ` +
        `${N} entries × ${S} bytes = ${expected}`,
    );
  }
}

// POLYDATA: the four topology-section byte sizes MUST be
// multiples of 4 (each cell entry is a uint32).
export function checkPolydata(msg: {
  size_vertices: number;
  size_lines: number;
  size_polygons: number;
  size_triangle_strips: number;
}): void {
  const sections: [string, number][] = [
    ["size_vertices", msg.size_vertices],
    ["size_lines", msg.size_lines],
    ["size_polygons", msg.size_polygons],
    ["size_triangle_strips", msg.size_triangle_strips],
  ];
  for (const [name, size] of sections) {
    if (size % 4 !== 0) {
      throw new BodyDecodeError(
        `POLYDATA: ${name}=${size} is not a multiple of 4`,
      );
    }
  }
}

// BIND: nametable_size must be even; len(bodies) must equal
// sum(ceil_to_even(body_size[i])) across header_entries.
export function checkBind(msg: {
  nametable_size: number;
  header_entries: ReadonlyArray<{ body_size: number | bigint }>;
  bodies: Uint8Array;
}): void {
  if (msg.nametable_size % 2 !== 0) {
    throw new BodyDecodeError(
      `BIND: nametable_size=${msg.nametable_size} must be even ` +
        `(2-byte aligned)`,
    );
  }
  let expected = 0n;
  for (const entry of msg.header_entries) {
    // body_size is uint64 on the wire, may be number or bigint
    // depending on field-plan width.
    const bs = BigInt(entry.body_size);
    expected += bs + (bs % 2n);  // pad odd to even
  }
  if (BigInt(msg.bodies.length) !== expected) {
    throw new BodyDecodeError(
      `BIND: bodies length ${msg.bodies.length} does not match ` +
        `sum(ceil_to_even(header_entries[i].body_size))=${expected}`,
    );
  }
}

// Dispatch table keyed by the schema's post_unpack_invariant string.
export const POST_UNPACK_INVARIANTS: Record<string, (msg: unknown) => void> = {
  ndarray: (msg) => checkNdarray(msg as Parameters<typeof checkNdarray>[0]),
  image: (msg) => checkImage(msg as Parameters<typeof checkImage>[0]),
  colortable: (msg) =>
    checkColortable(msg as Parameters<typeof checkColortable>[0]),
  polydata: (msg) =>
    checkPolydata(msg as Parameters<typeof checkPolydata>[0]),
  bind: (msg) => checkBind(msg as Parameters<typeof checkBind>[0]),
};
