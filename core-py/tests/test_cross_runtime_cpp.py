"""Cross-runtime integration — core-py Client ↔ core-cpp Server.

Spawns the ``oigtl_cpp_tcp_echo`` fixture (built from
``core-cpp/tests/fixtures/cpp_tcp_echo.cpp``), reads the random
listening port from its stdout, then round-trips a TRANSFORM
through core-py's :class:`~oigtl.net.Client` and asserts the C++
server's STATUS reply carries the translation fields we put on
the wire.

The whole point: prove byte compatibility across the core-py /
core-cpp boundary. If the wire encoders disagree on any field
layout, the C++ server's ``receive<Transform>()`` either fails or
decodes wrong values, and the reply's ``status_message`` won't
match the expected regex.

Skipped when the C++ binary hasn't been built — CI and local devs
without a core-cpp build still see a clean test suite.
"""

from __future__ import annotations

import asyncio
import os
import subprocess
from pathlib import Path
from typing import Any

import pytest

from datetime import timedelta

from oigtl.messages.status import Status
from oigtl.messages.transform import Transform
from oigtl.net import Client
from oigtl.net._options import ClientOptions


# ---------------------------------------------------------------------------
# Locate the C++ fixture binary
# ---------------------------------------------------------------------------


def _find_cpp_echo_binary() -> Path | None:
    """Walk up from this test file looking for a sibling core-cpp build.

    Returns the path to ``oigtl_cpp_tcp_echo`` if present, else None.
    """
    here = Path(__file__).resolve().parent
    for ancestor in [here, *here.parents]:
        candidate = ancestor / "core-cpp" / "build" / "oigtl_cpp_tcp_echo"
        if candidate.exists() and os.access(candidate, os.X_OK):
            return candidate
        # Windows suffix, just in case.
        candidate_exe = candidate.with_suffix(".exe")
        if candidate_exe.exists() and os.access(candidate_exe, os.X_OK):
            return candidate_exe
    return None


CPP_ECHO = _find_cpp_echo_binary()
CAN_RUN = CPP_ECHO is not None


# ---------------------------------------------------------------------------
# Fixture lifecycle — spawn the C++ server, read its port
# ---------------------------------------------------------------------------


async def _spawn_cpp_echo() -> tuple[asyncio.subprocess.Process, int]:
    """Start the C++ echo server and return (process, port).

    Raises RuntimeError if the binary doesn't emit PORT= within a
    generous timeout (server failed to bind, compile mismatch, etc.).
    """
    assert CPP_ECHO is not None  # guarded by CAN_RUN at call site
    proc = await asyncio.create_subprocess_exec(
        str(CPP_ECHO),
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )

    async def _read_port() -> int:
        assert proc.stdout is not None
        while True:
            line = await proc.stdout.readline()
            if not line:
                raise RuntimeError(
                    "cpp_tcp_echo exited before emitting PORT= "
                    f"(stderr: {await _stderr(proc)})"
                )
            text = line.decode("utf-8", errors="replace").rstrip()
            if text.startswith("PORT="):
                return int(text[len("PORT="):])
            # Ignore any unrelated log lines.

    async def _stderr(p: asyncio.subprocess.Process) -> str:
        if p.stderr is None:
            return "<no stderr>"
        data = await p.stderr.read()
        return data.decode("utf-8", errors="replace")

    try:
        port = await asyncio.wait_for(_read_port(), timeout=15.0)
    except Exception:
        proc.kill()
        await proc.wait()
        raise
    return proc, port


async def _stop(proc: asyncio.subprocess.Process) -> None:
    """Terminate the C++ fixture; SIGKILL if it won't go."""
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


@pytest.mark.skipif(not CAN_RUN, reason="oigtl_cpp_tcp_echo not built")
@pytest.mark.asyncio
async def test_transform_status_round_trip_py_to_cpp():
    """core-py Client → core-cpp Server → STATUS reply, byte-compatible."""
    proc, port = await _spawn_cpp_echo()
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
            # The C++ fixture formats translations as "X.0, Y.0, Z.0".
            assert "matrix[-3:]=[11.0, 22.0, 33.0]" in env.body.status_message
        finally:
            await c.close()
    finally:
        await _stop(proc)
