import assert from "node:assert/strict";
import { describe, it } from "node:test";

import {
  fromHex,
  readF32ArrayBE,
  readI16,
  readI64,
  readU16,
  readU32,
  readU64,
  toHex,
  viewOf,
  writeF32ArrayBE,
  writeI64,
  writeU16,
  writeU32,
  writeU64,
} from "../../src/runtime/byte_order.js";

describe("byte_order scalar readers", () => {
  it("reads big-endian uint16", () => {
    const bytes = new Uint8Array([0x12, 0x34]);
    assert.equal(readU16(viewOf(bytes), 0), 0x1234);
  });

  it("reads big-endian uint32", () => {
    const bytes = new Uint8Array([0x12, 0x34, 0x56, 0x78]);
    assert.equal(readU32(viewOf(bytes), 0), 0x12345678);
  });

  it("reads big-endian uint64 as bigint", () => {
    const bytes = fromHex("0102030405060708");
    assert.equal(readU64(viewOf(bytes), 0), 0x0102030405060708n);
  });

  it("reads signed 16 correctly (negative)", () => {
    const bytes = new Uint8Array([0xff, 0xfe]);
    assert.equal(readI16(viewOf(bytes), 0), -2);
  });

  it("reads signed 64 correctly", () => {
    const bytes = fromHex("ffffffffffffffff");
    assert.equal(readI64(viewOf(bytes), 0), -1n);
  });
});

describe("byte_order scalar writers", () => {
  it("writes uint16 BE", () => {
    const bytes = new Uint8Array(2);
    writeU16(viewOf(bytes), 0, 0x1234);
    assert.equal(toHex(bytes), "1234");
  });

  it("writes uint32 BE", () => {
    const bytes = new Uint8Array(4);
    writeU32(viewOf(bytes), 0, 0xdeadbeef);
    assert.equal(toHex(bytes), "deadbeef");
  });

  it("writes uint64 BE", () => {
    const bytes = new Uint8Array(8);
    writeU64(viewOf(bytes), 0, 0x0102030405060708n);
    assert.equal(toHex(bytes), "0102030405060708");
  });

  it("writes int64 BE (negative)", () => {
    const bytes = new Uint8Array(8);
    writeI64(viewOf(bytes), 0, -1n);
    assert.equal(toHex(bytes), "ffffffffffffffff");
  });
});

describe("bulk array readers/writers", () => {
  it("roundtrips a Float32Array through BE wire bytes", () => {
    const values = new Float32Array([1.0, 2.0, -3.5, 0.125]);
    const raw = new Uint8Array(values.length * 4);
    writeF32ArrayBE(viewOf(raw), 0, values);
    const out = readF32ArrayBE(viewOf(raw), 0, values.length);
    assert.deepEqual(Array.from(out), Array.from(values));
  });

  it("produces endian-correct wire bytes for float32", () => {
    const values = new Float32Array([1.0]);
    const raw = new Uint8Array(4);
    writeF32ArrayBE(viewOf(raw), 0, values);
    // 1.0 as big-endian float32 = 3F 80 00 00
    assert.equal(toHex(raw), "3f800000");
  });
});

describe("hex helpers", () => {
  it("round-trips bytes through hex", () => {
    const bytes = new Uint8Array([0x00, 0xff, 0x7f, 0x80]);
    assert.equal(toHex(bytes), "00ff7f80");
    assert.deepEqual(Array.from(fromHex("00ff7f80")), Array.from(bytes));
  });

  it("ignores whitespace in hex input", () => {
    assert.deepEqual(Array.from(fromHex("01 02\n03")), [1, 2, 3]);
  });

  it("rejects odd-length hex", () => {
    assert.throws(() => fromHex("abc"), /odd length/);
  });
});
