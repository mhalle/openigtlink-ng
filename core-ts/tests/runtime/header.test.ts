import assert from "node:assert/strict";
import { describe, it } from "node:test";

import { fromHex } from "../../src/runtime/byte_order.js";
import { CrcMismatchError } from "../../src/runtime/errors.js";
import { HEADER_SIZE, packHeader, unpackHeader, verifyCrc } from "../../src/runtime/header.js";

// Upstream TRANSFORM fixture — 58-byte header + 48-byte body.
// This is the same hex exported by
// `oigtl-corpus fixtures export-json`; keep in sync if the
// upstream changes.
const TRANSFORM_WIRE = fromHex(
  "0001" + // version
    "5452414e53464f524d000000" + // "TRANSFORM" + 3 NULs (12 bytes)
    "4465766963654e616d65" + // "DeviceName"
    "00000000000000000000" + // NUL padding to 20 bytes
    "00000000499602d4" + // timestamp
    "0000000000000030" + // body_size = 48
    "f6dd2b8eb4df6dd2" + // CRC
    "bf7473cd3e4959e6be63dd98be4959e63e12491b3f7852d6" + // body[0..24]
    "3e63dd983f7852d6bdc830ae42383660419bc46742383660", // body[24..48]
);

describe("unpackHeader", () => {
  it("parses version, type_id, device_name, body_size", () => {
    const h = unpackHeader(TRANSFORM_WIRE);
    assert.equal(h.version, 1);
    assert.equal(h.typeId, "TRANSFORM");
    assert.equal(h.deviceName, "DeviceName");
    assert.equal(h.bodySize, 48n);
  });

  it("strips trailing NUL padding from type_id", () => {
    const h = unpackHeader(TRANSFORM_WIRE);
    assert.ok(!h.typeId.includes("\x00"));
  });

  it("throws on short buffer", () => {
    assert.throws(
      () => unpackHeader(new Uint8Array(HEADER_SIZE - 1)),
      /need 58 bytes/,
    );
  });
});

describe("verifyCrc", () => {
  it("passes for the genuine fixture", () => {
    const h = unpackHeader(TRANSFORM_WIRE);
    const body = TRANSFORM_WIRE.subarray(HEADER_SIZE);
    verifyCrc(h, body); // should not throw
  });

  it("throws CrcMismatchError on a bad body", () => {
    const h = unpackHeader(TRANSFORM_WIRE);
    const badBody = new Uint8Array(48);
    assert.throws(
      () => verifyCrc(h, badBody),
      (e) => e instanceof CrcMismatchError,
    );
  });
});

describe("packHeader", () => {
  it("round-trips the TRANSFORM fixture byte-for-byte", () => {
    const original = unpackHeader(TRANSFORM_WIRE);
    const body = TRANSFORM_WIRE.subarray(HEADER_SIZE);
    const rebuilt = packHeader({
      version: original.version,
      typeId: original.typeId,
      deviceName: original.deviceName,
      timestamp: original.timestamp,
      body,
    });
    assert.deepEqual(
      Array.from(rebuilt),
      Array.from(TRANSFORM_WIRE.subarray(0, HEADER_SIZE)),
    );
  });

  it("rejects over-long type_id", () => {
    assert.throws(
      () =>
        packHeader({
          version: 1,
          typeId: "THIS_IS_WAY_TOO_LONG_FOR_12_BYTES",
          deviceName: "dev",
          timestamp: 0n,
          body: new Uint8Array(0),
        }),
      /type_id too long/,
    );
  });

  it("rejects non-ASCII in device_name", () => {
    assert.throws(
      () =>
        packHeader({
          version: 1,
          typeId: "TRANSFORM",
          deviceName: "Déviçe",
          timestamp: 0n,
          body: new Uint8Array(0),
        }),
      /ASCII-only/,
    );
  });
});
