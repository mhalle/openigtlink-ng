"""Cross-runtime integration — core-py WsClient ↔ core-ts WsServer.

Spawns the TS fixture (``core-ts/build-tests/tests/net/fixtures/
ts_ws_echo.js`` built from the sibling ``.ts``) via ``node``, reads
the random port from its stdout, and round-trips a TRANSFORM
through core-py's :class:`~oigtl.net.WsClient`. Asserts the reply's
``status_message`` carries the translation values we put on the
wire — byte-exact proof the py→ts-ws direction agrees on the
wire.

The fourth interop cell:

  - py  Client    → cpp TCP server  (test_cross_runtime_cpp.py)
  - cpp Client    → py  TCP server  (cross_runtime_py_test.cpp)
  - ts  WsClient  → py  WS  server  (cross_runtime.test.ts)
  - ts  Client    → cpp TCP server  (cross_runtime_cpp.test.ts)
  → py  WsClient  → ts  WS  server  (this file)

Skipped when:
  - The built TS fixture (``ts_ws_echo.js``) isn't present. Local
    devs without a core-ts ``npm test`` build see a clean skip.
  - ``node`` isn't on $PATH.
"""

from __future__ import annotations

import asyncio
import os
import shutil
from pathlib import Path

import pytest

from oigtl.messages.status import Status
from oigtl.messages.transform import Transform
from oigtl.net.ws_client import WsClient


# ---------------------------------------------------------------------------
# Locate the compiled TS fixture (`core-ts/build-tests/...`)
# ---------------------------------------------------------------------------


def _find_ts_ws_echo() -> Path | None:
    """Walk up from this test file looking for the compiled TS fixture.

    The TS test suite compiles its sources to ``core-ts/build-tests/``
    via ``tsc -p tsconfig.json``. We read that output directly rather
    than spawn ``tsc`` ourselves — leaves the build pipeline where it
    already is.
    """
    here = Path(__file__).resolve().parent
    relative = Path(
        "core-ts",
        "build-tests",
        "tests",
        "net",
        "fixtures",
        "ts_ws_echo.js",
    )
    for ancestor in [here, *here.parents]:
        candidate = ancestor / relative
        if candidate.exists():
            return candidate
    return None


TS_WS_ECHO = _find_ts_ws_echo()
NODE = shutil.which("node")
CAN_RUN = TS_WS_ECHO is not None and NODE is not None


# ---------------------------------------------------------------------------
# Fixture lifecycle
# ---------------------------------------------------------------------------


async def _spawn_ts_ws_echo() -> tuple[asyncio.subprocess.Process, int]:
    """Start the TS WS echo server and return (process, port)."""
    assert CAN_RUN
    assert NODE is not None and TS_WS_ECHO is not None
    proc = await asyncio.create_subprocess_exec(
        NODE, str(TS_WS_ECHO),
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
                    "ts_ws_echo exited before emitting PORT= "
                    f"(stderr: {stderr!r})"
                )
            text = line.decode("utf-8", errors="replace").rstrip()
            if text.startswith("PORT="):
                return int(text[len("PORT="):])
            # Ignore any unrelated log lines.

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


# ---------------------------------------------------------------------------
# The test
# ---------------------------------------------------------------------------


@pytest.mark.skipif(not CAN_RUN, reason=(
    "requires node on $PATH and core-ts build-tests compiled "
    "(run `npm test` in core-ts once)"
))
@pytest.mark.asyncio
async def test_transform_status_round_trip_py_ws_to_ts_ws():
    """core-py WsClient → core-ts WsServer → STATUS reply, byte-compatible."""
    proc, port = await _spawn_ts_ws_echo()
    try:
        c = await WsClient.connect(f"ws://127.0.0.1:{port}/")
        try:
            tx = Transform(matrix=[
                1, 0, 0,
                0, 1, 0,
                0, 0, 1,
                11, 22, 33,
            ])
            await c.send(tx)
            env = await c.receive(Status)
            assert env.body.code == 1
            # The TS fixture formats integer matrix translations as
            # "X.0, Y.0, Z.0" — matches the Python and C++ fixtures
            # so downstream tooling can reuse the same regex.
            assert "matrix[-3:]=[11.0, 22.0, 33.0]" in env.body.status_message
        finally:
            await c.close()
    finally:
        await _stop(proc)
