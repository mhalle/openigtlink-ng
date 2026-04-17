import assert from "node:assert/strict";
import { describe, it } from "node:test";

import { fromHex } from "../../src/runtime/byte_order.js";
import { parseWire } from "../../src/runtime/oracle.js";

const TRANSFORM_WIRE = fromHex(
  "0001" +
    "5452414e53464f524d000000" +
    "4465766963654e616d65" +
    "00000000000000000000" +
    "00000000499602d4" +
    "0000000000000030" +
    "f6dd2b8eb4df6dd2" +
    "bf7473cd3e4959e6be63dd98be4959e63e12491b3f7852d6" +
    "3e63dd983f7852d6bdc830ae42383660419bc46742383660",
);

describe("parseWire on the TRANSFORM fixture", () => {
  it("returns ok=true and populates header + content", () => {
    const r = parseWire(TRANSFORM_WIRE);
    assert.equal(r.ok, true);
    assert.equal(r.error, "");
    assert.equal(r.header?.typeId, "TRANSFORM");
    assert.equal(r.header?.version, 1);
    assert.equal(r.contentBytes.length, 48);
    assert.equal(r.extendedHeader, null);
    assert.equal(r.metadata.length, 0);
    assert.equal(r.extHeaderBytes.length, 0);
    assert.equal(r.metadataBytes.length, 0);
  });

  it("fails gracefully on CRC mismatch", () => {
    const corrupted = new Uint8Array(TRANSFORM_WIRE);
    corrupted[60] = (corrupted[60] ?? 0) ^ 0xff; // flip a body byte
    const r = parseWire(corrupted);
    assert.equal(r.ok, false);
    assert.match(r.error, /CRC mismatch/);
    assert.equal(r.header?.typeId, "TRANSFORM"); // header still parsed
  });

  it("fails gracefully on truncation", () => {
    const truncated = TRANSFORM_WIRE.subarray(0, 80); // < header + body
    const r = parseWire(truncated);
    assert.equal(r.ok, false);
    assert.match(r.error, /truncated/);
  });
});
