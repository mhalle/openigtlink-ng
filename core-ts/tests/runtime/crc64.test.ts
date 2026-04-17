import assert from "node:assert/strict";
import { describe, it } from "node:test";

import { crc64 } from "../../src/runtime/crc64.js";

const enc = new TextEncoder();

describe("crc64 ECMA-182 (OpenIGTLink table variant)", () => {
  it("matches the reference check value for '123456789'", () => {
    // The Python + C++ implementations both produce this value.
    assert.equal(crc64(enc.encode("123456789")), 0x6c40df5f0b497347n);
  });

  it("returns 0 for the empty buffer", () => {
    assert.equal(crc64(new Uint8Array(0)), 0n);
  });

  it("chains across chunks", () => {
    const full = crc64(enc.encode("hello world"));
    const chained = crc64(enc.encode(" world"), crc64(enc.encode("hello")));
    assert.equal(chained, full);
  });

  it("treats the seed as a starting remainder", () => {
    // Folding in zero bytes under a seed should act as a pure table
    // shift — not zero the result. Regression guard for accidental
    // `seed = 0n` hardcoding.
    const seed = 0xdeadbeefcafef00dn;
    assert.notEqual(crc64(new Uint8Array(0), seed), 0n);
    assert.equal(crc64(new Uint8Array(0), seed), seed);
  });

  it("handles long inputs", () => {
    // Build a 10 KiB alternating pattern and compare against
    // computing in halves.
    const buf = new Uint8Array(10_240);
    for (let i = 0; i < buf.length; i++) buf[i] = i & 0xff;
    const full = crc64(buf);
    const half = crc64(buf.subarray(5_120), crc64(buf.subarray(0, 5_120)));
    assert.equal(full, half);
  });

  it("split-uint32 path produces stable results for every byte value", () => {
    // Regression guard for the hi/lo uint32 arithmetic — catches
    // any sign-bit or >>> 0 slip that would corrupt the running CRC
    // for specific byte patterns.
    //
    // Strategy: feed each possible single-byte input and compare
    // against a known-good bigint reference implementation.
    const refCrc = (bytes: Uint8Array): bigint => {
      // Reference: verbatim bigint byte-at-a-time. Slow but obviously
      // correct.
      let crc = 0n;
      const mask = 0xffffffffffffffffn;
      const table = refTable();
      for (const b of bytes) {
        const idx = Number((crc >> 56n) & 0xffn) ^ b;
        crc = ((crc << 8n) & mask) ^ (table[idx] as bigint);
      }
      return crc;
    };
    for (let b = 0; b < 256; b++) {
      const buf = new Uint8Array([b]);
      assert.equal(crc64(buf), refCrc(buf), `byte 0x${b.toString(16)}`);
    }
  });

  it("matches reference over a 4 KiB random-ish buffer", () => {
    const buf = new Uint8Array(4096);
    // Linear congruential so the test is deterministic.
    let s = 0x12345678;
    for (let i = 0; i < buf.length; i++) {
      s = (s * 1103515245 + 12345) & 0x7fffffff;
      buf[i] = s & 0xff;
    }
    const ref = refCrc64(buf);
    assert.equal(crc64(buf), ref);
  });
});

// Reference bigint CRC-64 kept alongside the tests so a regression
// in the optimized path can be caught against an implementation
// whose only complexity is polynomial division.
function refTable(): readonly bigint[] {
  if (_cachedRefTable) return _cachedRefTable;
  const poly = 0x42f0e1eba9ea3693n;
  const mask = 0xffffffffffffffffn;
  const t: bigint[] = new Array(256);
  for (let i = 0; i < 256; i++) {
    let c = BigInt(i) << 56n;
    for (let k = 0; k < 8; k++) {
      c = c & (1n << 63n) ? ((c << 1n) & mask) ^ poly : (c << 1n) & mask;
    }
    t[i] = c;
  }
  _cachedRefTable = t;
  return t;
}
let _cachedRefTable: readonly bigint[] | undefined;

function refCrc64(bytes: Uint8Array): bigint {
  const table = refTable();
  const mask = 0xffffffffffffffffn;
  let crc = 0n;
  for (const b of bytes) {
    const idx = Number((crc >> 56n) & 0xffn) ^ b;
    crc = ((crc << 8n) & mask) ^ (table[idx] as bigint);
  }
  return crc;
}
