#!/usr/bin/env python3
"""Python-side fixture for the C++ cross-runtime TCP interop test.

Starts an :class:`oigtl.net.Server` on a random free port, prints
``PORT=<n>\\n`` (flushed) to stdout so the C++ test can read it,
then serves exactly:

- TRANSFORM → reply with STATUS(status_message="got matrix[-3:]=[x, y, z]")
  in the same format as ``python_ws_echo.py`` and ``cpp_tcp_echo.cpp``
  use, so the parent test can reuse the same regex.

After replying, keeps the connection open (the C++ test closes it)
and keeps serving additional TRANSFORMs from subsequent peers.

Exits on SIGTERM / SIGINT.

The complement to ``cpp_tcp_echo.cpp``: that fixture proves the
core-py Client can drive a core-cpp Server correctly. This one
proves the reverse — a core-cpp Client can drive a core-py Server
correctly — completing the py↔cpp interop matrix.
"""

from __future__ import annotations

import asyncio
import signal
from typing import Any

from oigtl.messages import Status, Transform
from oigtl.net import Server


async def main() -> None:
    server = await Server.listen(0, host="127.0.0.1")
    # Port on its own line, flushed — matches the python_ws_echo.py
    # and cpp_tcp_echo.cpp protocol.
    print(f"PORT={server.port}", flush=True)

    @server.on(Transform)
    async def _on_transform(env: Any, peer: Any) -> None:
        last3 = env.body.matrix[-3:]
        await peer.send(Status(
            code=1,
            sub_code=0,
            error_name="",
            status_message=f"got matrix[-3:]={last3}",
        ))

    # Tear down cleanly when the parent signals SIGTERM. Windows
    # note is identical to python_ws_echo.py — asyncio's
    # add_signal_handler raises NotImplementedError there and
    # terminate() maps to TerminateProcess, so we never see the
    # handler anyway. Silently skip on Windows.
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
