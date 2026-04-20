# /// script
# requires-python = ">=3.11"
# dependencies = ["oigtl"]
# ///
"""Companion Python server for ``browser_ws_demo.html``.

Listens on ``ws://0.0.0.0:18945/`` — matches the default URL
baked into the HTML. Replies to every TRANSFORM with a STATUS
message that echoes the translation component ``matrix[-3:]``, so
you can visually confirm the round trip in the browser log.

Run from the repo root so the editable ``oigtl`` install is
picked up::

    uv run --project core-py python core-ts/examples/demo_ws_server.py

Then open ``core-ts/examples/browser_ws_demo.html`` in a browser
(served via ``python3 -m http.server`` from ``core-ts/`` or via
any Vite-style dev server that can reach the built ``dist/``).

Exits cleanly on Ctrl+C.
"""

from __future__ import annotations

import asyncio
from typing import Any

from oigtl.messages import Status, Transform
from oigtl.net import Server


PORT = 18945


async def main() -> None:
    server = await Server.listen_ws(PORT)
    print(
        f"[demo] listening on ws://127.0.0.1:{server.port}/  "
        f"(open browser_ws_demo.html, click Connect)"
    )

    @server.on(Transform)
    async def _on_transform(env: Any, peer: Any) -> None:
        last3 = env.body.matrix[-3:]
        await peer.send(Status(
            code=1,
            sub_code=0,
            error_name="",
            status_message=f"got matrix[-3:]={last3}",
        ))

    @server.on_connected
    def _on_up(peer: Any) -> None:
        print(f"[demo] peer connected: {peer.address}")

    @server.on_disconnected
    def _on_down(peer: Any, cause: Exception | None) -> None:
        note = f" ({cause})" if cause is not None else ""
        print(f"[demo] peer disconnected: {peer.address}{note}")

    try:
        await server.serve()
    except KeyboardInterrupt:
        pass
    finally:
        await server.close()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
