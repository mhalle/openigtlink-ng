/**
 * Cross-language oracle parity.
 *
 * For every upstream fixture, run both the TS oracle and the Python
 * oracle and assert their JSON reports match on the stable subset
 * of fields (ok, type_id, version, body_size, ext_header_size,
 * metadata_count, round_trip_ok).
 *
 * Python is invoked via `uv run oigtl-corpus oracle verify --fixture`.
 * Tests are skipped when `uv` isn't on PATH (e.g. on CI runners that
 * only install Node tooling).
 */

import assert from "node:assert/strict";
import { execFileSync, spawnSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
import { describe, it } from "node:test";

import { fromHex } from "../src/runtime/byte_order.js";
import { toReport, verifyWireBytes } from "../src/runtime/oracle.js";
import "../src/messages/index.js";

function uvAvailable(): boolean {
  const r = spawnSync("uv", ["--version"], { stdio: "pipe" });
  return r.status === 0;
}

function repoRoot(): string {
  const here = dirname(fileURLToPath(import.meta.url));
  // Walk up until we find a directory containing `spec/` and `corpus-tools/`.
  let dir = here;
  for (let i = 0; i < 6; i++) {
    try {
      readFileSync(resolve(dir, "corpus-tools/pyproject.toml"), "utf-8");
      return dir;
    } catch {
      dir = resolve(dir, "..");
    }
  }
  throw new Error("could not locate repo root from " + here);
}

function loadFixtures(): Record<
  string,
  { type_id: string; wire_hex: string }
> {
  const here = dirname(fileURLToPath(import.meta.url));
  const candidates = [
    resolve(here, "../../../spec/corpus/upstream-fixtures.json"),
    resolve(here, "../spec/corpus/upstream-fixtures.json"),
    resolve(here, "../../spec/corpus/upstream-fixtures.json"),
  ];
  for (const c of candidates) {
    try {
      const parsed = JSON.parse(readFileSync(c, "utf-8"));
      return parsed.fixtures;
    } catch {
      /* next */
    }
  }
  throw new Error("upstream-fixtures.json not found");
}

describe("cross-language oracle parity (TS vs Python)", () => {
  if (!uvAvailable()) {
    it("skipped: uv not on PATH", () => {
      // The test exists so the suite runs the describe; this `it`
      // documents the skip reason in test output.
    });
    return;
  }

  const root = repoRoot();
  const fixtures = loadFixtures();

  for (const [name, fx] of Object.entries(fixtures)) {
    it(`${name}`, () => {
      // TS side
      const wire = fromHex(fx.wire_hex);
      const tsResult = toReport(verifyWireBytes(wire));

      // Python side
      const pyStdout = execFileSync(
        "uv",
        [
          "run",
          "--project",
          "corpus-tools",
          "oigtl-corpus",
          "oracle",
          "verify",
          "--fixture",
          name,
        ],
        { cwd: root, stdio: ["ignore", "pipe", "pipe"] },
      ).toString();
      const pyReport = JSON.parse(pyStdout);

      // Some fixtures have no typed class in TS (framing helpers);
      // Python's oracle has the same gap. In both cases `ok` is false
      // with "no typed class" or similar — tolerate by only comparing
      // when both succeeded.
      if (!tsResult.ok || !pyReport.ok) {
        assert.equal(
          tsResult.ok,
          pyReport.ok,
          `TS ok=${tsResult.ok} (${tsResult.error}) vs Py ok=${pyReport.ok} (${pyReport.error})`,
        );
        return;
      }

      const keys: (keyof typeof tsResult)[] = [
        "type_id",
        "device_name",
        "version",
        "body_size",
        "ext_header_size",
        "metadata_count",
        "round_trip_ok",
      ];
      for (const k of keys) {
        assert.equal(
          tsResult[k],
          pyReport[k],
          `${name}.${String(k)} mismatch: TS=${JSON.stringify(tsResult[k])} Py=${JSON.stringify(pyReport[k])}`,
        );
      }
    });
  }
});
