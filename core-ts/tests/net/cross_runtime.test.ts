/**
 * Cross-runtime integration test — TS WsClient ↔ Python Server.listen_ws.
 *
 * Spawns the Python helper fixture (which uses ``uv run`` to pick
 * up the core-py project), reads the assigned port from stdout,
 * then connects with {@link WsClient}, sends a TRANSFORM, and
 * verifies the STATUS reply.
 *
 * The whole point of this test: prove the wire is byte-identical
 * across language boundaries without relying on either side's
 * assumptions. If this passes, a browser written against
 * ``@openigtlink/core/net/ws`` can talk to a Python
 * ``Server.listen_ws`` deployment in production with the same
 * confidence.
 *
 * Skipped if ``uv`` isn't on the $PATH — so local developers
 * without uv don't see a failure, and CI (which has uv) runs it.
 */

import assert from "node:assert/strict";
import { spawn, spawnSync, type ChildProcess } from "node:child_process";
import { existsSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { describe, it } from "node:test";
import { setTimeout as delay } from "node:timers/promises";
import { fileURLToPath } from "node:url";

import WebSocket from "ws";

import { WsClient, type WebSocketLikeCtor } from "../../src/net/ws/index.js";
import "../../src/messages/index.js";
import { Status } from "../../src/messages/status.js";
import { Transform } from "../../src/messages/transform.js";

const WsCtor = WebSocket as unknown as WebSocketLikeCtor;

// ---------------------------------------------------------------------------
// Environment probe — skip the suite unless uv + core-py are available.
// ---------------------------------------------------------------------------

function findCorePyRoot(): string | null {
  // Walk upward from this test file until we find a sibling
  // `core-py/pyproject.toml`. Robust against source vs. build-tests
  // layout differences (ts source at tests/..., compiled js at
  // build-tests/tests/..., different depths).
  let dir = dirname(fileURLToPath(import.meta.url));
  for (let i = 0; i < 8; i++) {
    const candidate = resolve(dir, "core-py", "pyproject.toml");
    if (existsSync(candidate)) return dirname(candidate);
    const parent = dirname(dir);
    if (parent === dir) break;
    dir = parent;
  }
  return null;
}

function hasUv(): boolean {
  const res = spawnSync("uv", ["--version"], { stdio: "ignore" });
  return res.status === 0;
}

function findFixture(name: string): string | null {
  // Source lives in tests/net/fixtures; compiled tests sit in
  // build-tests/tests/net/ and don't carry the .py file with them.
  // Find the repo root via core-py's pyproject, then reach into
  // core-ts/tests/net/fixtures.
  const corePy = findCorePyRoot();
  if (corePy === null) return null;
  const repoRoot = dirname(corePy);
  const path = resolve(repoRoot, "core-ts", "tests", "net", "fixtures", name);
  return existsSync(path) ? path : null;
}

const CORE_PY_ROOT = findCorePyRoot();
const FIXTURE_PATH = findFixture("python_ws_echo.py");
const CAN_RUN =
  hasUv() && CORE_PY_ROOT !== null && FIXTURE_PATH !== null;

// ---------------------------------------------------------------------------
// Fixture lifecycle
// ---------------------------------------------------------------------------

interface PythonHarness {
  proc: ChildProcess;
  port: number;
  stop(): Promise<void>;
}

function spawnPython(): Promise<PythonHarness> {
  return new Promise((resolve_, reject) => {
    const proc: ChildProcess = spawn(
      "uv",
      ["run", "--project", CORE_PY_ROOT!, "python", FIXTURE_PATH!],
      { stdio: ["ignore", "pipe", "pipe"] },
    );

    let stdoutBuf = "";
    let settled = false;
    const timer = setTimeout(() => {
      if (!settled) {
        settled = true;
        try { proc.kill("SIGKILL"); } catch { /* noop */ }
        reject(new Error("python fixture didn't emit PORT within 15s"));
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
            // Wait up to 3s for graceful exit, then SIGKILL.
            const exited = new Promise<void>((r) => proc.once("exit", () => r()));
            await Promise.race([exited, delay(3000)]);
            if (!proc.killed) {
              try { proc.kill("SIGKILL"); } catch { /* noop */ }
            }
          },
        });
      }
    });

    let stderrBuf = "";
    proc.stderr?.on("data", (chunk: string) => {
      stderrBuf += chunk;
    });
    proc.once("exit", (code: number | null) => {
      if (!settled) {
        settled = true;
        clearTimeout(timer);
        reject(new Error(
          `python fixture exited code=${code} before emitting PORT.\n` +
          `stderr: ${stderrBuf}`,
        ));
      }
    });
  });
}

// ---------------------------------------------------------------------------
// The actual test
// ---------------------------------------------------------------------------

describe("cross-runtime: TS WsClient ↔ Python Server.listen_ws", () => {
  it(
    "TRANSFORM → STATUS round-trip across language boundary",
    { skip: !CAN_RUN },
    async () => {
      const harness = await spawnPython();
      try {
        const c = await WsClient.connect(
          `ws://127.0.0.1:${harness.port}/`,
          { webSocket: WsCtor, connectTimeoutMs: 5000 },
        );
        try {
          const tx = new Transform({
            matrix: [1, 0, 0, 0, 1, 0, 0, 0, 1, 11, 22, 33],
          });
          await c.send(tx);
          const reply = await c.receive(Status, { timeoutMs: 5000 });
          assert.ok(reply.body instanceof Status);
          assert.equal(reply.body.code, 1);
          // The Python fixture echoes the last 3 matrix values
          // back in the status_message — byte-exact proof the
          // server decoded our TRANSFORM correctly.
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
      const harness = await spawnPython();
      try {
        const c = await WsClient.connect(
          `ws://127.0.0.1:${harness.port}/`,
          { webSocket: WsCtor, connectTimeoutMs: 5000 },
        );
        try {
          for (let i = 1; i <= 4; i++) {
            await c.send(new Transform({
              matrix: [1,0,0, 0,1,0, 0,0,1, i, i * 2, i * 3],
            }));
            const reply = await c.receive(Status, { timeoutMs: 5000 });
            assert.match(
              reply.body.status_message,
              new RegExp(`matrix\\[-3:\\]=\\[${i}\\.0, ${i * 2}\\.0, ${i * 3}\\.0\\]`),
            );
          }
        } finally {
          await c.close();
        }
      } finally {
        await harness.stop();
      }
    },
  );
});
