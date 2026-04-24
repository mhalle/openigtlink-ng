"""Tests for the synchronous :class:`~oigtl.net.SyncClient` wrapper.

The sync surface delegates every operation to the async client
running on a shared background loop. We test the researcher-facing
contract: blocking semantics, correct result values, exception
propagation, and iteration via ``messages()``.

The loopback server is driven by a small threading-based harness
since this test file must stay sync itself (it's the sync API).
"""

from __future__ import annotations

import asyncio
import threading
import time
from datetime import timedelta

import pytest

from oigtl.messages import Status, Transform
from oigtl.net import Client, ClientOptions, SyncClient
from oigtl.net.errors import ConnectionClosedError
from oigtl.net.errors import TimeoutError as NetTimeoutError
from oigtl.runtime.header import HEADER_SIZE, pack_header, unpack_header


# ---------------------------- server harness ---------------------------


class ThreadedLoopbackServer:
    """asyncio server driven on its own thread for sync-test use."""

    def __init__(self, handler):
        self._handler = handler
        self._ready = threading.Event()
        self._thread = threading.Thread(
            target=self._run, name="test-loopback", daemon=True,
        )
        self._loop: asyncio.AbstractEventLoop | None = None
        self._server: asyncio.base_events.Server | None = None
        self._port: int = 0

    def start(self) -> int:
        self._thread.start()
        self._ready.wait()
        return self._port

    def stop(self) -> None:
        if self._loop is not None and self._loop.is_running():
            self._loop.call_soon_threadsafe(self._loop.stop)
        self._thread.join(timeout=5)

    def _run(self) -> None:
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)

        async def on_peer(reader, writer):
            try:
                await self._handler(reader, writer)
            finally:
                writer.close()
                try:
                    await writer.wait_closed()
                except Exception:
                    pass

        async def boot():
            self._server = await asyncio.start_server(
                on_peer, host="127.0.0.1", port=0,
            )
            assert self._server.sockets
            self._port = self._server.sockets[0].getsockname()[1]
            self._ready.set()

        self._loop.run_until_complete(boot())
        try:
            self._loop.run_forever()
        finally:
            if self._server is not None:
                self._server.close()
            self._loop.close()


async def _read_one(reader):
    header = unpack_header(await reader.readexactly(HEADER_SIZE))
    body = await reader.readexactly(header.body_size)
    return header.type_id, body


def _write_frame(writer, type_id, body, *, device_name="srv"):
    # version=1: body is bare (no v2 extended-header region).
    header = pack_header(
        version=1, type_id=type_id, device_name=device_name,
        timestamp=0, body=body,
    )
    writer.write(header + body)


# --------------------------- tests ------------------------------------


def test_sync_connect_and_close():
    async def handler(reader, writer):
        await _read_one(reader)

    srv = ThreadedLoopbackServer(handler)
    port = srv.start()
    try:
        c = SyncClient.connect("127.0.0.1", port)
        try:
            assert c.peer is not None
            assert c.peer[1] == port
        finally:
            c.close()
    finally:
        srv.stop()


def test_sync_send_and_receive():
    async def handler(reader, writer):
        type_id, _ = await _read_one(reader)
        assert type_id == "TRANSFORM"
        _write_frame(writer, "STATUS",
                     Status(code=1, sub_code=0, error_name="",
                            status_message="sync-ok").pack())
        await writer.drain()
        # Keep the connection open long enough for the client to
        # read the reply.
        await asyncio.sleep(1.0)

    srv = ThreadedLoopbackServer(handler)
    port = srv.start()
    try:
        with SyncClient.connect("127.0.0.1", port) as c:
            c.send(Transform(matrix=[1,0,0,0,1,0,0,0,1,0,0,0]))
            env = c.receive(Status, timeout=2)
            assert env.body.status_message == "sync-ok"
    finally:
        srv.stop()


def test_sync_context_manager_closes():
    async def handler(reader, writer):
        await asyncio.sleep(2)

    srv = ThreadedLoopbackServer(handler)
    port = srv.start()
    try:
        with SyncClient.connect("127.0.0.1", port) as c:
            pass
        # After context exit, send fails.
        with pytest.raises(ConnectionClosedError):
            c.send(Transform())
    finally:
        srv.stop()


def test_sync_receive_timeout_raises():
    async def handler(reader, writer):
        await asyncio.sleep(5)

    srv = ThreadedLoopbackServer(handler)
    port = srv.start()
    try:
        with SyncClient.connect("127.0.0.1", port) as c:
            with pytest.raises(NetTimeoutError):
                c.receive(Status, timeout=timedelta(milliseconds=100))
    finally:
        srv.stop()


def test_sync_messages_iterator():
    async def handler(reader, writer):
        await _read_one(reader)   # consume client's kickoff
        for i in range(3):
            _write_frame(writer, "STATUS",
                         Status(code=1, sub_code=0, error_name="",
                                status_message=f"msg-{i}").pack())
        await writer.drain()
        # Let the client drain, then close so iteration terminates.
        await asyncio.sleep(0.3)

    srv = ThreadedLoopbackServer(handler)
    port = srv.start()
    try:
        with SyncClient.connect("127.0.0.1", port) as c:
            c.send(Transform())
            collected = []
            # Each iter blocks up to 2 s; peer close ends the loop.
            for env in c.messages(timeout=2):
                collected.append(env.body.status_message)
                if len(collected) == 3:
                    break
            assert collected == ["msg-0", "msg-1", "msg-2"]
    finally:
        srv.stop()


def test_connect_sync_classmethod_on_async_client():
    """``Client.connect_sync()`` is the documented sync entry point."""
    async def handler(reader, writer):
        await asyncio.sleep(1)

    srv = ThreadedLoopbackServer(handler)
    port = srv.start()
    try:
        c = Client.connect_sync("127.0.0.1", port)
        try:
            assert isinstance(c, SyncClient)
            assert c.peer is not None
        finally:
            c.close()
    finally:
        srv.stop()


def test_sync_preserves_exception_types():
    """Exceptions raised in the async layer propagate unchanged."""
    opt = ClientOptions(connect_timeout=timedelta(milliseconds=100))
    with pytest.raises((NetTimeoutError, ConnectionClosedError)):
        # TEST-NET-2, guaranteed unrouted.
        SyncClient.connect("198.51.100.1", 1, opt)


def test_sync_duration_coercion():
    """Unit-bearing duration spellings all yield the same timedelta."""
    # Bare number on the canonical name → seconds.
    opt_seconds = ClientOptions(receive_timeout=0.25)
    # Milliseconds via the _ms companion.
    opt_ms = ClientOptions(receive_timeout_ms=250)
    # Explicit timedelta.
    opt_td = ClientOptions(receive_timeout=timedelta(milliseconds=250))
    assert opt_seconds.receive_timeout == opt_ms.receive_timeout
    assert opt_ms.receive_timeout == opt_td.receive_timeout
