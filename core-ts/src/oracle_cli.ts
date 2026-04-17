/**
 * oracle_cli — persistent oracle subprocess for differential fuzzing.
 *
 * Reads hex-encoded wire-byte strings from stdin (one per line) and
 * emits a JSON oracle report to stdout for each. EOF terminates
 * cleanly. Mirrors the C++ `oigtl_oracle_cli` binary and the Python
 * oracle `verify` command — all three emit identical JSON shapes,
 * which is the point: the `oigtl-corpus fuzz differential` runner
 * compares them line-for-line.
 *
 * Run with (after `npm run build`):
 *
 *     node dist/oracle_cli.js
 *
 * Or during development (after `tsc -p tsconfig.json --outDir build-tests`):
 *
 *     node build-tests/src/oracle_cli.js
 */

import { createInterface } from "node:readline";
import { stdin, stdout } from "node:process";

import { fromHex } from "./runtime/byte_order.js";
import { toReport, verifyWireBytes } from "./runtime/oracle.js";
import "./messages/index.js"; // side-effect: registers all 84 message classes

function emitInvalidHex(): void {
  // Emit a report with the same shape as a real failure — keeps the
  // Python consumer simple (no special-case parsing for hex errors).
  const empty = {
    ok: false,
    type_id: "",
    device_name: "",
    version: 0,
    body_size: 0,
    ext_header_size: null,
    metadata_count: 0,
    round_trip_ok: false,
    error: "oracle_cli: invalid hex input",
  };
  stdout.write(JSON.stringify(empty) + "\n");
}

const rl = createInterface({ input: stdin, crlfDelay: Infinity });

rl.on("line", (line) => {
  const clean = line.trim();
  let bytes: Uint8Array;
  try {
    bytes = fromHex(clean);
  } catch {
    emitInvalidHex();
    return;
  }
  try {
    const result = verifyWireBytes(bytes);
    stdout.write(JSON.stringify(toReport(result)) + "\n");
  } catch (e) {
    // Unexpected exception — should never happen (the oracle swallows
    // its own errors), but surface rather than crash so a real bug
    // gets recorded in the fuzzer's disagreement log.
    const err = {
      ok: false,
      type_id: "",
      device_name: "",
      version: 0,
      body_size: 0,
      ext_header_size: null,
      metadata_count: 0,
      round_trip_ok: false,
      error: `oracle_cli: uncaught ${(e as Error).name}: ${(e as Error).message}`,
    };
    stdout.write(JSON.stringify(err) + "\n");
  }
});

rl.on("close", () => {
  // EOF on stdin — exit cleanly.
  process.exit(0);
});
