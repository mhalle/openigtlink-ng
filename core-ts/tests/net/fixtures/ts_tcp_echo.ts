/**
 * Cross-runtime test fixture — a TS TCP echo server.
 *
 * Mirrors `python_tcp_echo.py`, `cpp_tcp_echo.cpp`, and the
 * sibling `ts_ws_echo.ts`: binds a random free port on 127.0.0.1,
 * prints `PORT=<n>\n` (flushed) to stdout, then serves
 *
 *   TRANSFORM → reply with STATUS carrying the last 3 matrix
 *               values in `status_message`, in the canonical
 *               "got matrix[-3:]=[X.0, Y.0, Z.0]" format every
 *               cross-runtime test regex-matches.
 *
 * Stays open until SIGTERM. Drives py → ts-tcp and cpp → ts-tcp
 * interop tests.
 *
 * Runs as compiled JavaScript out of `build-tests/`.
 */

import "../../../src/messages/index.js";   // register built-ins
import { Status } from "../../../src/messages/status.js";
import { Transform } from "../../../src/messages/transform.js";
import { Server } from "../../../src/net/server.js";

async function main(): Promise<void> {
  const server = await Server.listen(0, { host: "127.0.0.1" });
  process.stdout.write(`PORT=${server.port}\n`);

  server.on(Transform, async (env, peer) => {
    const last3 = env.body.matrix.slice(-3);
    const fmt = (v: number): string =>
      Number.isInteger(v) ? `${v}.0` : String(v);
    await peer.send(new Status({
      code: 1,
      subcode: 0n,
      error_name: "",
      status_message: `got matrix[-3:]=[${last3.map(fmt).join(", ")}]`,
    }));
  });

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
  console.error("ts_tcp_echo: fatal:", err);
  process.exit(1);
});
