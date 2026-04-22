"""Cross-runtime integration — core-py Client ↔ core-ts Server (TCP).

Spawns the TS fixture (``core-ts/build-tests/tests/net/fixtures/
ts_tcp_echo.js``) via ``node``, reads the random port from its
stdout, and round-trips a TRANSFORM through core-py's
:class:`~oigtl.net.Client` TCP transport. Asserts the reply's
``status_message`` carries the translation values we put on the
wire.

Completes the pairwise TCP matrix: every core can now be exercised
as both client and server (TCP) against every other core.

Skipped when:
  - The built TS fixture isn't present (no `npm test` in core-ts yet).
  - ``node`` isn't on $PATH.
"""

from __future__ import annotations

import asyncio
import shutil
from datetime import timedelta
from pathlib import Path

import pytest

from oigtl.messages.status import Status
from oigtl.messages.transform import Transform
from oigtl.net import Client
from oigtl.net._options import ClientOptions


def _find_ts_tcp_echo() -> Path | None:
    """Walk up looking for the compiled TS TCP echo fixture."""
    here = Path(__file__).resolve().parent
    relative = Path(
        "core-ts",
        "build-tests",
        "tests",
        "net",
        "fixtures",
        "ts_tcp_echo.js",
    )
    for ancestor in [here, *here.parents]:
        candidate = ancestor / relative
        if candidate.exists():
            return candidate
    return None


TS_TCP_ECHO = _find_ts_tcp_echo()
NODE = shutil.which("node")
CAN_RUN = TS_TCP_ECHO is not None and NODE is not None


async def _spawn_ts_tcp_echo() -> tuple[asyncio.subprocess.Process, int]:
    assert CAN_RUN
    assert NODE is not None and TS_TCP_ECHO is not None
    proc = await asyncio.create_subprocess_exec(
        NODE, str(TS_TCP_ECHO),
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )

    async def _read_port() -> int:
        assert proc.stdout is not None
        while True:
            line = await proc.stdout.readline()
            if not line:
                stderr = b""
                if proc.stderr is not None:
                    stderr = await proc.stderr.read()
                raise RuntimeError(
                    f"ts_tcp_echo exited before emitting PORT= "
                    f"(stderr: {stderr!r})"
                )
            text = line.decode("utf-8", errors="replace").rstrip()
            if text.startswith("PORT="):
                return int(text[len("PORT="):])

    try:
        port = await asyncio.wait_for(_read_port(), timeout=15.0)
    except Exception:
        proc.kill()
        await proc.wait()
        raise
    return proc, port


async def _stop(proc: asyncio.subprocess.Process) -> None:
    if proc.returncode is not None:
        return
    proc.terminate()
    try:
        await asyncio.wait_for(proc.wait(), timeout=3.0)
    except asyncio.TimeoutError:
        proc.kill()
        await proc.wait()


@pytest.mark.skipif(not CAN_RUN, reason=(
    "requires node on $PATH and core-ts build-tests compiled "
    "(run `npm test` in core-ts once)"
))
@pytest.mark.asyncio
async def test_transform_status_round_trip_py_tcp_to_ts_tcp():
    """core-py Client → core-ts Server (TCP) → STATUS reply, byte-compatible."""
    proc, port = await _spawn_ts_tcp_echo()
    try:
        opts = ClientOptions(connect_timeout=timedelta(seconds=5))
        c = await Client.connect("127.0.0.1", port, opts)
        try:
            tx = Transform(matrix=[
                1, 0, 0,
                0, 1, 0,
                0, 0, 1,
                11, 22, 33,
            ])
            await c.send(tx)
            env = await c.receive(Status, timeout=timedelta(seconds=5))
            assert env.body.code == 1
            # Shared "X.0, Y.0, Z.0" format across every fixture.
            assert "matrix[-3:]=[11.0, 22.0, 33.0]" in env.body.status_message
        finally:
            await c.close()
    finally:
        await _stop(proc)
