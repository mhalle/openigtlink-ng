/**
 * Cross-runtime integration — core-ts `Client` ↔ core-cpp TCP server.
 *
 * Spawns `oigtl_cpp_tcp_echo` (built from
 * `core-cpp/tests/fixtures/cpp_tcp_echo.cpp`), reads the random
 * listening port from its stdout, and round-trips a TRANSFORM
 * through core-ts's TCP {@link Client}. Asserts the C++ server's
 * STATUS reply carries the translation fields we put on the wire
 * — byte-exact proof that the ts-to-cpp TCP path works across
 * the language boundary.
 *
 * Complements:
 *   - `cross_runtime.test.ts`        — ts WsClient  ↔ py WS server
 *   - `test_cross_runtime_cpp.py`    — py Client    ↔ cpp TCP server
 *
 * With this test, all three pairwise directions we can reach
 * without building a cpp WebSocket server are exercised in CI.
 *
 * Skipped if the core-cpp fixture binary isn't built — local
 * developers who haven't run CMake on core-cpp still see a clean
 * suite; CI builds the fixture explicitly.
 */

import assert from "node:assert/strict";
import { spawn, type ChildProcess } from "node:child_process";
import { existsSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { describe, it } from "node:test";
import { setTimeout as delay } from "node:timers/promises";
import { fileURLToPath } from "node:url";

import { Client } from "../../src/net/client.js";
import "../../src/messages/index.js";   // side effect: register types
import { Status } from "../../src/messages/status.js";
import { Transform } from "../../src/messages/transform.js";

// ---------------------------------------------------------------------------
// Locate the C++ echo fixture binary
// ---------------------------------------------------------------------------

function findCppEchoBinary(): string | null {
  // Walk upward from this test file until we find the sibling
  // `core-cpp/build/oigtl_cpp_tcp_echo`. Robust against the
  // source (tests/net/) vs. build-tests (build-tests/tests/net/)
  // layouts.
  let dir = dirname(fileURLToPath(import.meta.url));
  for (let i = 0; i < 8; i++) {
    const candidate = resolve(dir, "core-cpp", "build", "oigtl_cpp_tcp_echo");
    if (existsSync(candidate)) return candidate;
    const windowsCandidate = candidate + ".exe";
    if (existsSync(windowsCandidate)) return windowsCandidate;
    const parent = dirname(dir);
    if (parent === dir) break;
    dir = parent;
  }
  return null;
}

const CPP_ECHO = findCppEchoBinary();
const CAN_RUN = CPP_ECHO !== null;

// ---------------------------------------------------------------------------
// Fixture lifecycle — spawn the C++ server and read its port
// ---------------------------------------------------------------------------

interface CppHarness {
  proc: ChildProcess;
  port: number;
  stop(): Promise<void>;
}

function spawnCppEcho(): Promise<CppHarness> {
  return new Promise((resolve_, reject) => {
    if (CPP_ECHO === null) {
      reject(new Error("cpp fixture path not resolved"));
      return;
    }
    const proc = spawn(CPP_ECHO, [], { stdio: ["ignore", "pipe", "pipe"] });

    let stdoutBuf = "";
    let settled = false;
    const timer = setTimeout(() => {
      if (!settled) {
        settled = true;
        try { proc.kill("SIGKILL"); } catch { /* noop */ }
        reject(new Error("cpp fixture didn't emit PORT within 15s"));
      }
    }, 15000);

    proc.stdout?.setEncoding("utf8");
    proc.stderr?.setEncoding("utf8");
    proc.stdout?.on("data", (chunk: string) => {
      if (settled) return;
      stdoutBuf += chunk;
      const line = stdoutBuf.split("\n").find((l) => l.startsWith("PORT="));
      if (line !== undefined) {
        settled = true;
        clearTimeout(timer);
        const port = Number.parseInt(line.slice(5), 10);
        resolve_({
          proc,
          port,
          stop: async () => {
            try { proc.kill("SIGTERM"); } catch { /* noop */ }
            const exited = new Promise<void>((r) =>
              proc.once("exit", () => r()),
            );
            await Promise.race([exited, delay(3000)]);
            if (!proc.killed) {
              try { proc.kill("SIGKILL"); } catch { /* noop */ }
            }
          },
        });
      }
    });

    let stderrBuf = "";
    proc.stderr?.on("data", (chunk: string) => { stderrBuf += chunk; });
    proc.once("exit", (code: number | null) => {
      if (!settled) {
        settled = true;
        clearTimeout(timer);
        reject(new Error(
          `cpp fixture exited code=${code} before emitting PORT.\n` +
          `stderr: ${stderrBuf}`,
        ));
      }
    });
  });
}

// ---------------------------------------------------------------------------
// The actual test
// ---------------------------------------------------------------------------

describe("cross-runtime: TS Client ↔ core-cpp TCP server", () => {
  it(
    "TRANSFORM → STATUS round-trip across the language boundary",
    { skip: !CAN_RUN },
    async () => {
      const harness = await spawnCppEcho();
      try {
        const c = await Client.connect("127.0.0.1", harness.port, {
          connectTimeoutMs: 5000,
        });
        try {
          const tx = new Transform({
            matrix: [1, 0, 0, 0, 1, 0, 0, 0, 1, 11, 22, 33],
          });
          await c.send(tx);
          const reply = await c.receive(Status, { timeoutMs: 5000 });
          assert.ok(reply.body instanceof Status);
          assert.equal(reply.body.code, 1);
          // The C++ fixture formats translations as "X.0, Y.0, Z.0"
          // and echoes them in status_message — byte-exact proof
          // the server decoded our TRANSFORM correctly.
          assert.match(
            reply.body.status_message,
            /matrix\[-3:\]=\[11\.0, 22\.0, 33\.0\]/,
          );
        } finally {
          await c.close();
        }
      } finally {
        await harness.stop();
      }
    },
  );

  it(
    "multiple round-trips over one connection",
    { skip: !CAN_RUN },
    async () => {
      const harness = await spawnCppEcho();
      try {
        // The fixture is one-shot (accepts one peer, serves one
        // TRANSFORM, exits). For a multi-round-trip sanity check,
        // spawn a fresh fixture per iteration — each exercises the
        // full accept → decode → encode → close path.
        //
        // This test exists more for its shape (parallel to
        // cross_runtime.test.ts's "multiple round-trips" case) than
        // because it finds anything the single-round-trip doesn't.
        // If the C++ fixture grows a multi-message mode later, this
        // should follow suit.
        for (let i = 1; i <= 2; i++) {
          const c = await Client.connect("127.0.0.1", harness.port, {
            connectTimeoutMs: 5000,
          });
          try {
            await c.send(new Transform({
              matrix: [1, 0, 0, 0, 1, 0, 0, 0, 1, i, i * 2, i * 3],
            }));
            const reply = await c.receive(Status, { timeoutMs: 5000 });
            assert.match(
              reply.body.status_message,
              new RegExp(
                `matrix\\[-3:\\]=\\[${i}\\.0, ${i * 2}\\.0, ${i * 3}\\.0\\]`,
              ),
            );
          } finally {
            await c.close();
          }
          // The fixture exits after one round-trip; next iteration
          // needs its own spawn.
          if (i < 2) {
            await harness.stop();
            Object.assign(harness, await spawnCppEcho());
          }
        }
      } finally {
        await harness.stop();
      }
    },
  );
});
