/**
 * Cross-runtime test fixture — a TS WebSocket echo server.
 *
 * Mirrors `python_ws_echo.py` and `cpp_tcp_echo.cpp`: binds a
 * random free port on 127.0.0.1, prints `PORT=<n>\n` (flushed) to
 * stdout so the parent test can read it, then serves
 *
 *   TRANSFORM → reply with STATUS carrying the last 3 matrix
 *               values in `status_message`, in the canonical
 *               "got matrix[-3:]=[X.0, Y.0, Z.0]" format every
 *               cross-runtime test regex-matches.
 *
 * Unlike the C++ fixture (one-shot), this fixture stays open for
 * additional peers until SIGTERM. The Python test drives it for
 * one round-trip and then terminates.
 *
 * Runs as compiled JavaScript out of `build-tests/` — spawn
 * `node build-tests/tests/net/fixtures/ts_ws_echo.js` from the
 * parent.
 */

import "../../../src/messages/index.js";   // register built-ins
import { Status } from "../../../src/messages/status.js";
import { Transform } from "../../../src/messages/transform.js";
import { WsServer } from "../../../src/net/ws_server.js";

async function main(): Promise<void> {
  const server = await WsServer.listen(0, { host: "127.0.0.1" });

  // Flush PORT so the parent reads it promptly.
  process.stdout.write(`PORT=${server.port}\n`);

  server.on(Transform, async (env, peer) => {
    const last3 = env.body.matrix.slice(-3);
    // Format translations as "X.0, Y.0, Z.0" — matches the Python
    // and C++ fixtures so the same regex works across all three.
    const fmt = (v: number): string => {
      if (Number.isInteger(v)) return `${v}.0`;
      return String(v);
    };
    const msg = `got matrix[-3:]=[${last3.map(fmt).join(", ")}]`;
    await peer.send(new Status({
      code: 1,
      subcode: 0n,
      error_name: "",
      status_message: msg,
    }));
  });

  // Clean shutdown on SIGTERM / SIGINT. Node handles these itself;
  // asyncio's Windows NotImplementedError dance isn't a factor here.
  const stop = async () => {
    await server.close();
    process.exit(0);
  };
  process.once("SIGINT", stop);
  process.once("SIGTERM", stop);

  await server.serve();
}

main().catch((err) => {
  // eslint-disable-next-line no-console
  console.error("ts_ws_echo: fatal:", err);
  process.exit(1);
});
