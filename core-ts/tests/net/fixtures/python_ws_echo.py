#!/usr/bin/env python3
"""Python-side fixture for the TS cross-runtime WS test.

Starts a ``Server.listen_ws`` on a random free port, prints
``PORT=<n>\\n`` to stdout (so the Node test can read it), then
serves:

- TRANSFORM → reply with STATUS(status_message="got matrix[-3:]=[x, y, z]")
- Any other known type → echo it back byte-exact via raw send

Exits on SIGTERM / SIGINT, or when the parent process closes stdin.
"""

from __future__ import annotations

import asyncio
import signal
from typing import Any

from oigtl.messages import Status, Transform
from oigtl.net import Server


async def main() -> None:
    server = await Server.listen_ws(0)
    # Print the port on its own line so the Node test can parse it.
    # flush=True matters — the pipe is line-buffered from the
    # parent's side only after flush.
    print(f"PORT={server.port}", flush=True)

    @server.on(Transform)
    async def _on_transform(env: Any, peer: Any) -> None:
        last3 = env.body.matrix[-3:]
        await peer.send(Status(
            code=1,
            sub_code=0,
            error_name="",
            status_message=(
                f"got matrix[-3:]={last3}"
            ),
        ))

    # Tear down cleanly when the parent signals SIGTERM (Node kills
    # the child at test teardown).
    #
    # Windows note: asyncio's ``add_signal_handler`` raises
    # ``NotImplementedError`` on Windows. That's fine — Node's
    # ``proc.kill("SIGTERM")`` on Windows maps to TerminateProcess,
    # which kills the interpreter outright; we never get a chance
    # to run a handler anyway. Silently skip registration and rely
    # on hard-kill teardown semantics there.
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, stop.set)
        except NotImplementedError:
            pass

    serve_task = asyncio.create_task(server.serve())

    await stop.wait()
    await server.close()
    try:
        await serve_task
    except Exception:
        pass


if __name__ == "__main__":
    asyncio.run(main())
