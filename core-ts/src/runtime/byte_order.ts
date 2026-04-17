/**
 * Big-endian byte-order readers and writers.
 *
 * OpenIGTLink is big-endian on the wire. `DataView` is the only
 * built-in API that lets us specify endianness per-read; JavaScript
 * TypedArrays (`Int16Array`, `Float32Array`, ...) are host-endian
 * and reading bytes through them is silently wrong on all real
 * hardware (which is little-endian).
 *
 * This module wraps `DataView` with named big-endian accessors so
 * generated codec code stays readable and every read has the
 * endian flag pinned.
 *
 * Conventions:
 *   - All readers take `(view, offset)` and return the value.
 *   - All writers take `(view, offset, value)` and mutate the view.
 *   - 64-bit integer types go through `bigint`; everything else is
 *     `number`.
 *   - Bulk array readers allocate a native-endian typed array and
 *     copy values through `DataView` — see {@link readFloat32ArrayBE}.
 *     Direct reinterpretation of the wire bytes as `Float32Array`
 *     would return garbage on LE hosts.
 */

// ---------------------------------------------------------------------------
// Scalar readers
// ---------------------------------------------------------------------------

export const readU8 = (v: DataView, o: number): number => v.getUint8(o);
export const readI8 = (v: DataView, o: number): number => v.getInt8(o);

export const readU16 = (v: DataView, o: number): number => v.getUint16(o, false);
export const readI16 = (v: DataView, o: number): number => v.getInt16(o, false);

export const readU32 = (v: DataView, o: number): number => v.getUint32(o, false);
export const readI32 = (v: DataView, o: number): number => v.getInt32(o, false);

export const readU64 = (v: DataView, o: number): bigint => v.getBigUint64(o, false);
export const readI64 = (v: DataView, o: number): bigint => v.getBigInt64(o, false);

export const readF32 = (v: DataView, o: number): number => v.getFloat32(o, false);
export const readF64 = (v: DataView, o: number): number => v.getFloat64(o, false);

// ---------------------------------------------------------------------------
// Scalar writers
// ---------------------------------------------------------------------------

export const writeU8 = (v: DataView, o: number, x: number): void => v.setUint8(o, x);
export const writeI8 = (v: DataView, o: number, x: number): void => v.setInt8(o, x);

export const writeU16 = (v: DataView, o: number, x: number): void =>
  v.setUint16(o, x, false);
export const writeI16 = (v: DataView, o: number, x: number): void =>
  v.setInt16(o, x, false);

export const writeU32 = (v: DataView, o: number, x: number): void =>
  v.setUint32(o, x, false);
export const writeI32 = (v: DataView, o: number, x: number): void =>
  v.setInt32(o, x, false);

export const writeU64 = (v: DataView, o: number, x: bigint): void =>
  v.setBigUint64(o, x, false);
export const writeI64 = (v: DataView, o: number, x: bigint): void =>
  v.setBigInt64(o, x, false);

export const writeF32 = (v: DataView, o: number, x: number): void =>
  v.setFloat32(o, x, false);
export const writeF64 = (v: DataView, o: number, x: number): void =>
  v.setFloat64(o, x, false);

// ---------------------------------------------------------------------------
// Bulk readers: big-endian wire bytes → native-endian typed array
// ---------------------------------------------------------------------------
//
// The result is a freshly-allocated TypedArray in host-endian order,
// so downstream code can feed it to WebGL / Canvas / charts without
// another copy. Cost is O(N) bytes read once through DataView.

export function readU16ArrayBE(v: DataView, offset: number, count: number): Uint16Array {
  const out = new Uint16Array(count);
  for (let i = 0; i < count; i++) out[i] = v.getUint16(offset + i * 2, false);
  return out;
}

export function readI16ArrayBE(v: DataView, offset: number, count: number): Int16Array {
  const out = new Int16Array(count);
  for (let i = 0; i < count; i++) out[i] = v.getInt16(offset + i * 2, false);
  return out;
}

export function readU32ArrayBE(v: DataView, offset: number, count: number): Uint32Array {
  const out = new Uint32Array(count);
  for (let i = 0; i < count; i++) out[i] = v.getUint32(offset + i * 4, false);
  return out;
}

export function readI32ArrayBE(v: DataView, offset: number, count: number): Int32Array {
  const out = new Int32Array(count);
  for (let i = 0; i < count; i++) out[i] = v.getInt32(offset + i * 4, false);
  return out;
}

export function readU64ArrayBE(v: DataView, offset: number, count: number): BigUint64Array {
  const out = new BigUint64Array(count);
  for (let i = 0; i < count; i++) out[i] = v.getBigUint64(offset + i * 8, false);
  return out;
}

export function readI64ArrayBE(v: DataView, offset: number, count: number): BigInt64Array {
  const out = new BigInt64Array(count);
  for (let i = 0; i < count; i++) out[i] = v.getBigInt64(offset + i * 8, false);
  return out;
}

export function readF32ArrayBE(v: DataView, offset: number, count: number): Float32Array {
  const out = new Float32Array(count);
  for (let i = 0; i < count; i++) out[i] = v.getFloat32(offset + i * 4, false);
  return out;
}

export function readF64ArrayBE(v: DataView, offset: number, count: number): Float64Array {
  const out = new Float64Array(count);
  for (let i = 0; i < count; i++) out[i] = v.getFloat64(offset + i * 8, false);
  return out;
}

// ---------------------------------------------------------------------------
// Bulk writers: native-endian typed array → big-endian wire bytes
// ---------------------------------------------------------------------------

export function writeU16ArrayBE(
  v: DataView,
  offset: number,
  values: ArrayLike<number>,
): void {
  for (let i = 0; i < values.length; i++) v.setUint16(offset + i * 2, values[i] as number, false);
}

export function writeI16ArrayBE(
  v: DataView,
  offset: number,
  values: ArrayLike<number>,
): void {
  for (let i = 0; i < values.length; i++) v.setInt16(offset + i * 2, values[i] as number, false);
}

export function writeU32ArrayBE(
  v: DataView,
  offset: number,
  values: ArrayLike<number>,
): void {
  for (let i = 0; i < values.length; i++) v.setUint32(offset + i * 4, values[i] as number, false);
}

export function writeI32ArrayBE(
  v: DataView,
  offset: number,
  values: ArrayLike<number>,
): void {
  for (let i = 0; i < values.length; i++) v.setInt32(offset + i * 4, values[i] as number, false);
}

export function writeU64ArrayBE(
  v: DataView,
  offset: number,
  values: ArrayLike<bigint>,
): void {
  for (let i = 0; i < values.length; i++)
    v.setBigUint64(offset + i * 8, values[i] as bigint, false);
}

export function writeI64ArrayBE(
  v: DataView,
  offset: number,
  values: ArrayLike<bigint>,
): void {
  for (let i = 0; i < values.length; i++)
    v.setBigInt64(offset + i * 8, values[i] as bigint, false);
}

export function writeF32ArrayBE(
  v: DataView,
  offset: number,
  values: ArrayLike<number>,
): void {
  for (let i = 0; i < values.length; i++) v.setFloat32(offset + i * 4, values[i] as number, false);
}

export function writeF64ArrayBE(
  v: DataView,
  offset: number,
  values: ArrayLike<number>,
): void {
  for (let i = 0; i < values.length; i++) v.setFloat64(offset + i * 8, values[i] as number, false);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Create a DataView spanning `bytes` exactly (respecting byteOffset). */
export function viewOf(bytes: Uint8Array): DataView {
  return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
}

/** Hex-encode a Uint8Array. Used in tests and error messages. */
export function toHex(bytes: Uint8Array): string {
  let out = "";
  for (let i = 0; i < bytes.length; i++) {
    out += (bytes[i] as number).toString(16).padStart(2, "0");
  }
  return out;
}

/** Parse a hex string into Uint8Array. Whitespace is ignored. */
export function fromHex(hex: string): Uint8Array {
  const clean = hex.replace(/\s+/g, "");
  if (clean.length % 2 !== 0) {
    throw new Error(`hex string has odd length: ${clean.length}`);
  }
  const out = new Uint8Array(clean.length / 2);
  for (let i = 0; i < out.length; i++) {
    const byte = Number.parseInt(clean.substring(i * 2, i * 2 + 2), 16);
    if (Number.isNaN(byte)) throw new Error(`invalid hex at offset ${i * 2}`);
    out[i] = byte;
  }
  return out;
}
