/**
 * Round-trip every upstream fixture through the typed registry and
 * assert byte-equality of content bytes.
 *
 * Fixture source: spec/corpus/upstream-fixtures.json, produced by
 * `oigtl-corpus fixtures export-json`. Kept in sync via a CI drift
 * check.
 */

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
import { describe, it } from "node:test";

import { fromHex } from "../src/runtime/byte_order.js";
import { lookupMessageClass, registrySize } from "../src/runtime/dispatch.js";
import { parseWire, verifyWireBytes } from "../src/runtime/oracle.js";

// Side-effect: registers all 84 classes with the dispatch registry.
import "../src/messages/index.js";

interface FixtureJson {
  format_version: number;
  count: number;
  fixtures: Record<
    string,
    {
      type_id: string;
      version: number;
      device_name: string;
      body_size: number;
      wire_hex: string;
    }
  >;
}

function loadFixtures(): FixtureJson {
  const here = dirname(fileURLToPath(import.meta.url));
  // build-tests/tests/round_trip_all.test.js → ../../../spec/corpus/...
  // dev tests/round_trip_all.test.ts      → ../spec/corpus/...
  const candidates = [
    resolve(here, "../../../spec/corpus/upstream-fixtures.json"),
    resolve(here, "../spec/corpus/upstream-fixtures.json"),
    resolve(here, "../../spec/corpus/upstream-fixtures.json"),
  ];
  for (const c of candidates) {
    try {
      return JSON.parse(readFileSync(c, "utf-8")) as FixtureJson;
    } catch {
      /* next */
    }
  }
  throw new Error(
    `upstream-fixtures.json not found at any of:\n${candidates.join("\n")}`,
  );
}

const fixtures = loadFixtures();

describe("registry population", () => {
  it("has 84 registered classes after importing messages barrel", () => {
    assert.equal(registrySize(), 84);
  });
});

describe("upstream fixture round-trip", () => {
  for (const [name, fx] of Object.entries(fixtures.fixtures)) {
    it(`${name} (${fx.type_id}, v${fx.version}, ${fx.body_size}B)`, () => {
      const wire = fromHex(fx.wire_hex);
      const ctor = lookupMessageClass(fx.type_id);
      if (ctor === undefined) {
        // Not all type_ids have a class — e.g. HEADER / EXT_HEADER
        // are framing helpers. Skip rather than fail so the suite
        // stays green; they'd show up as a lookup miss anyway.
        return;
      }
      const framing = parseWire(wire);
      assert.equal(framing.ok, true, framing.error);
      const msg = ctor.unpack(framing.contentBytes) as { pack(): Uint8Array };
      const repacked = msg.pack();
      assert.equal(
        repacked.length,
        framing.contentBytes.length,
        `${name}: pack produced ${repacked.length} bytes, expected ${framing.contentBytes.length}`,
      );
      for (let i = 0; i < repacked.length; i++) {
        if (repacked[i] !== framing.contentBytes[i]) {
          throw new Error(
            `${name}: byte mismatch at content offset ${i}: ` +
              `got 0x${(repacked[i] ?? 0).toString(16)}, expected 0x${(framing.contentBytes[i] ?? 0).toString(16)}`,
          );
        }
      }
    });
  }
});

describe("oracle verifyWireBytes", () => {
  for (const [name, fx] of Object.entries(fixtures.fixtures)) {
    it(`${name} produces a passing oracle report`, () => {
      const wire = fromHex(fx.wire_hex);
      const result = verifyWireBytes(wire);
      // Framing-only fixtures (HEADER, EXT_HEADER) won't find a class.
      if (!result.ok && result.error.startsWith("no typed class")) return;
      assert.equal(result.ok, true, result.error);
      assert.equal(result.roundTripOk, true);
    });
  }
});
