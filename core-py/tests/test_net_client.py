"""End-to-end tests for the async :class:`~oigtl.net.Client`.

Uses an in-process asyncio loopback server rather than mocks so we
exercise the real framing + CRC + socket paths. Phase 2 only covers
the happy path; resilience tests live in Phase 4.
"""

from __future__ import annotations

import asyncio
from datetime import timedelta

import pytest

from oigtl.messages import Status, Transform
from oigtl.net import Client, ClientOptions, Envelope
from oigtl.net.errors import ConnectionClosedError
from oigtl.net.errors import TimeoutError as NetTimeoutError
from oigtl.runtime.header import HEADER_SIZE, pack_header, unpack_header


# ----------------------------- helpers ---------------------------------


class _LoopbackServer:
    """Minimal asyncio server that echoes typed messages back.

    Each connection task reads one frame, parses the type, and uses
    the test's configured reply callback to produce the response.
    Good enough for happy-path testing without dragging in the
    Phase-5 Server implementation.
    """

    def __init__(self, handler):
        self._handler = handler
        self._server: asyncio.base_events.Server | None = None
        self._tasks: set[asyncio.Task] = set()

    async def start(self) -> int:
        self._server = await asyncio.start_server(
            self._on_peer, host="127.0.0.1", port=0,
        )
        sockets = self._server.sockets or ()
        assert sockets, "server has no listening sockets"
        return sockets[0].getsockname()[1]

    async def _on_peer(
        self,
        reader: asyncio.StreamReader,
        writer: asyncio.StreamWriter,
    ) -> None:
        try:
            await self._handler(reader, writer)
        finally:
            writer.close()
            try:
                await writer.wait_closed()
            except Exception:
                pass

    async def stop(self) -> None:
        if self._server is not None:
            self._server.close()
            await self._server.wait_closed()


async def _read_one_frame(
    reader: asyncio.StreamReader,
) -> tuple[str, bytes]:
    """Read one full IGTL frame from *reader*; return (type_id, body)."""
    header_bytes = await reader.readexactly(HEADER_SIZE)
    header = unpack_header(header_bytes)
    body = await reader.readexactly(header.body_size)
    return header.type_id, body


def _write_frame(
    writer: asyncio.StreamWriter,
    type_id: str,
    body: bytes,
    *,
    device_name: str = "srv",
) -> None:
    # version=1: body is bare (no v2 extended-header region).
    header = pack_header(
        version=1,
        type_id=type_id,
        device_name=device_name,
        timestamp=0,
        body=body,
    )
    writer.write(header + body)


# ----------------------------- fixtures --------------------------------


@pytest.fixture
async def echo_transform_server():
    """Accept one connection, read one TRANSFORM, reply with STATUS(ok)."""
    async def handler(reader, writer):
        type_id, body = await _read_one_frame(reader)
        assert type_id == "TRANSFORM"
        status = Status(code=1, sub_code=0, error_name="",
                        status_message="OK")
        _write_frame(writer, "STATUS", status.pack(), device_name="echo")
        await writer.drain()

    srv = _LoopbackServer(handler)
    port = await srv.start()
    try:
        yield port
    finally:
        await srv.stop()


# --------------------------- connect / close ---------------------------


async def test_connect_and_close(echo_transform_server):
    port = echo_transform_server
    c = await Client.connect("127.0.0.1", port)
    try:
        assert c.peer is not None
        assert c.peer[1] == port
    finally:
        await c.close()


async def test_connect_timeout_raises():
    # 198.51.100.0/24 is TEST-NET-2 — guaranteed unrouted.
    opt = ClientOptions(connect_timeout=timedelta(milliseconds=100))
    with pytest.raises((NetTimeoutError, ConnectionClosedError)):
        await Client.connect("198.51.100.1", 1, opt)


async def test_async_context_manager_closes(echo_transform_server):
    port = echo_transform_server
    c = await Client.connect("127.0.0.1", port)
    async with c:
        pass
    # After exit, send() should fail.
    with pytest.raises(ConnectionClosedError):
        await c.send(Transform())


# --------------------------- send / receive ----------------------------


async def test_send_and_receive_typed(echo_transform_server):
    port = echo_transform_server
    async with await Client.connect("127.0.0.1", port) as c:
        await c.send(Transform(matrix=[1,0,0,0,1,0,0,0,1,0,0,0]))
        env = await c.receive(Status)
        assert isinstance(env, Envelope)
        assert isinstance(env.body, Status)
        assert env.body.code == 1
        assert env.body.status_message == "OK"
        assert env.header.type_id == "STATUS"
        assert env.header.device_name == "echo"


async def test_receive_any_yields_raw_envelope(echo_transform_server):
    port = echo_transform_server
    async with await Client.connect("127.0.0.1", port) as c:
        await c.send(Transform())
        env = await c.receive_any()
        assert env.header.type_id == "STATUS"
        assert isinstance(env.body, Status)


async def test_receive_timeout_raises():
    # Server accepts but never replies.
    async def handler(reader, writer):
        await asyncio.sleep(5)

    srv = _LoopbackServer(handler)
    port = await srv.start()
    try:
        async with await Client.connect("127.0.0.1", port) as c:
            with pytest.raises(NetTimeoutError):
                await c.receive(Status, timeout=timedelta(milliseconds=100))
    finally:
        await srv.stop()


async def test_send_after_peer_close_raises():
    async def handler(reader, writer):
        # Close immediately — simulates peer hanging up.
        writer.close()

    srv = _LoopbackServer(handler)
    port = await srv.start()
    try:
        c = await Client.connect("127.0.0.1", port)
        # Give the peer-close FIN a moment to surface. Then the
        # second send should fail.
        await asyncio.sleep(0.05)
        # First send may or may not succeed depending on buffer state;
        # at most two sends in a tight loop should hit the error.
        with pytest.raises(ConnectionClosedError):
            for _ in range(5):
                await c.send(Transform())
                await asyncio.sleep(0.01)
        await c.close()
    finally:
        await srv.stop()


# --------------------------- dispatch loop -----------------------------


async def test_dispatch_decorator_routes_by_type():
    """Multiple message types interleaved; each routes to its @on handler."""
    async def handler(reader, writer):
        # Wait for the client's TRANSFORM, then stream three replies.
        await _read_one_frame(reader)
        _write_frame(writer, "TRANSFORM",
                     Transform(matrix=[1,0,0,0,1,0,0,0,1,1,2,3]).pack())
        _write_frame(writer, "STATUS",
                     Status(code=1, sub_code=0, error_name="",
                            status_message="alive").pack())
        _write_frame(writer, "TRANSFORM",
                     Transform(matrix=[1,0,0,0,1,0,0,0,1,4,5,6]).pack())
        await writer.drain()

    srv = _LoopbackServer(handler)
    port = await srv.start()
    try:
        async with await Client.connect("127.0.0.1", port) as c:
            transforms: list[Transform] = []
            statuses: list[Status] = []

            @c.on(Transform)
            async def _(env):
                transforms.append(env.body)

            @c.on(Status)
            async def _(env):
                statuses.append(env.body)

            await c.send(Transform())
            # Drive the dispatch loop until the server's stream ends.
            await asyncio.wait_for(c.run(), timeout=2.0)

        assert len(transforms) == 2
        assert len(statuses) == 1
        assert transforms[0].matrix[-3:] == [1.0, 2.0, 3.0]
        assert transforms[1].matrix[-3:] == [4.0, 5.0, 6.0]
        assert statuses[0].status_message == "alive"
    finally:
        await srv.stop()


async def test_receive_dispatches_intermediate_messages():
    """receive(T) drops unrelated types to their registered handlers."""
    async def handler(reader, writer):
        await _read_one_frame(reader)
        # Send STATUS first (unregistered in the client, will be dropped)
        # then TRANSFORM (the target).
        _write_frame(writer, "STATUS",
                     Status(code=1, sub_code=0, error_name="",
                            status_message="ignored").pack())
        _write_frame(writer, "TRANSFORM",
                     Transform(matrix=[1,0,0,0,1,0,0,0,1,9,9,9]).pack())
        await writer.drain()

    srv = _LoopbackServer(handler)
    port = await srv.start()
    try:
        async with await Client.connect("127.0.0.1", port) as c:
            seen_status: list[Status] = []

            @c.on(Status)
            async def _(env):
                seen_status.append(env.body)

            await c.send(Transform())
            env = await c.receive(Transform, timeout=2.0)
            assert env.body.matrix[-3:] == [9.0, 9.0, 9.0]
            # The STATUS was dispatched to its handler en route.
            assert len(seen_status) == 1
            assert seen_status[0].status_message == "ignored"
    finally:
        await srv.stop()


async def test_options_accept_int_ms():
    """Duration coercion: int ms and timedelta produce equal timeouts."""
    opt1 = ClientOptions(connect_timeout=500)
    opt2 = ClientOptions(connect_timeout=timedelta(milliseconds=500))
    assert opt1.connect_timeout == opt2.connect_timeout
