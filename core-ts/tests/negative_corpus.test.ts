/**
 * Negative corpus — every must-reject entry is rejected by the TS codec.
 *
 * Shares data with Python/C++ via ``spec/corpus/negative/index.json``.
 * Entries flagged ``currently_accepted_by: ["ts", ...]`` are known
 * codec gaps tracked in ``security/PLAN.md`` — they stay in the
 * corpus as behaviour documentation; once the codec learns to
 * reject them, the annotation is removed and the test flips.
 */

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
import { describe, it } from "node:test";

import { verifyWireBytes } from "../src/runtime/oracle.js";
import "../src/messages/index.js";

interface NegativeEntry {
  path: string;
  description: string;
  error_class: string;
  spec_reference: string;
  size_bytes: number;
  known_issue?: string;
  currently_accepted_by?: string[];
}

function findRepoRoot(): string {
  const here = dirname(fileURLToPath(import.meta.url));
  let dir = here;
  for (let i = 0; i < 6; i++) {
    try {
      readFileSync(resolve(dir, "spec/corpus/negative/index.json"), "utf-8");
      return dir;
    } catch {
      dir = resolve(dir, "..");
    }
  }
  throw new Error("repo root not found from " + here);
}

const root = findRepoRoot();
const negDir = resolve(root, "spec/corpus/negative");
const index = JSON.parse(readFileSync(resolve(negDir, "index.json"), "utf-8"));
const entries: Record<string, NegativeEntry> = index.entries;

describe("negative corpus rejection", () => {
  for (const [name, entry] of Object.entries(entries)) {
    const acceptedBy = entry.currently_accepted_by ?? [];
    if (acceptedBy.includes("ts")) {
      // Known gap — record the xfail intent in the test name. Node's
      // test runner has no xfail primitive; we assert the current
      // (wrong) behaviour so a future fix breaks the test and
      // prompts removal of the annotation.
      it(`${name} (XFAIL: ${entry.known_issue ?? "known gap"})`, () => {
        const data = new Uint8Array(readFileSync(resolve(negDir, entry.path)));
        const result = verifyWireBytes(data);
        assert.equal(
          result.ok,
          true,
          `${name} is annotated as a known gap (currently accepted) ` +
            `but TS actually rejected it with: ${result.error}. ` +
            `Remove "ts" from currently_accepted_by in the generator.`,
        );
      });
      continue;
    }
    it(`${name}`, () => {
      const data = new Uint8Array(readFileSync(resolve(negDir, entry.path)));
      const result = verifyWireBytes(data);
      assert.equal(
        result.ok,
        false,
        `${name}: expected rejection with error_class=${entry.error_class}, ` +
          `got ok=true (decoded as ${result.header?.typeId ?? "?"})`,
      );
    });
  }
});
