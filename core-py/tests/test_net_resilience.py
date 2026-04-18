"""Client resilience tests — Phase 4.

Mirrors the four tests in the C++ ``ergo_test.cpp`` resilience
block:

1. Offline buffer disabled → ``send()`` during outage raises.
2. ``DropOldest`` policy discards head, send still succeeds.
3. ``DropNewest`` policy raises :class:`BufferOverflowError`.
4. ``reconnect_max_attempts`` exhausted → client becomes terminal.

Plus coverage for:

- Happy-path auto-reconnect (server comes back, buffer drains).
- ``on_connected`` / ``on_disconnected`` / ``on_reconnect_failed``
  callbacks fire with correct arguments.
- Backoff schedule shape (compute_backoff is a pure function, easy
  to test deterministically).
- Keepalive helper doesn't error on our platforms.
"""

from __future__ import annotations

import asyncio
import random
import socket
from datetime import timedelta

import pytest

from oigtl.messages import Status, Transform
from oigtl.net import (
    Client,
    ClientOptions,
    OfflineOverflow,
)
from oigtl.net._resilience import compute_backoff, configure_keepalive
from oigtl.net.errors import BufferOverflowError, ConnectionClosedError
from oigtl.runtime.header import HEADER_SIZE, pack_header, unpack_header


# ---------------- helper: startable/restartable echo server -----------


class RestartableServer:
    """A single-port server that can accept → close → re-accept.

    ``stop()`` force-closes every active peer connection, so tests
    can simulate "tracker vanishes" without waiting for in-flight
    handlers to end voluntarily.
    """

    def __init__(self, handler):
        self._handler = handler
        self._server: asyncio.base_events.Server | None = None
        self._port: int | None = None
        self._peers: set[asyncio.StreamWriter] = set()

    async def start(self, *, port: int | None = None) -> int:
        async def on_peer(reader, writer):
            self._peers.add(writer)
            try:
                await self._handler(reader, writer)
            except Exception:
                pass
            finally:
                self._peers.discard(writer)
                writer.close()
                try:
                    await writer.wait_closed()
                except Exception:
                    pass

        self._server = await asyncio.start_server(
            on_peer,
            host="127.0.0.1",
            port=port or 0,
            reuse_address=True,
        )
        assert self._server.sockets
        self._port = self._server.sockets[0].getsockname()[1]
        return self._port

    async def stop(self) -> None:
        if self._server is None:
            return
        self._server.close()
        # Force-close active peers so wait_closed() doesn't block.
        for w in list(self._peers):
            try:
                w.close()
            except Exception:
                pass
        try:
            await asyncio.wait_for(
                self._server.wait_closed(), timeout=2,
            )
        except asyncio.TimeoutError:
            pass
        self._server = None
        self._peers.clear()


async def _drain_one_frame(reader):
    header = unpack_header(await reader.readexactly(HEADER_SIZE))
    body = await reader.readexactly(header.body_size)
    return header.type_id, body


# -------- backoff schedule (pure function, deterministic) -------------


def test_compute_backoff_doubles_without_jitter():
    opt = ClientOptions(
        auto_reconnect=True,
        reconnect_initial_backoff=timedelta(milliseconds=100),
        reconnect_max_backoff=timedelta(seconds=10),
        reconnect_backoff_jitter=0.0,
    )
    delays = [compute_backoff(a, opt).total_seconds() for a in range(1, 6)]
    assert delays == [0.1, 0.2, 0.4, 0.8, 1.6]


def test_compute_backoff_caps_at_max():
    opt = ClientOptions(
        auto_reconnect=True,
        reconnect_initial_backoff=timedelta(milliseconds=100),
        reconnect_max_backoff=timedelta(milliseconds=300),
        reconnect_backoff_jitter=0.0,
    )
    delays = [compute_backoff(a, opt).total_seconds() for a in range(1, 8)]
    assert max(delays) == pytest.approx(0.3, abs=1e-9)


def test_compute_backoff_applies_jitter():
    opt = ClientOptions(
        auto_reconnect=True,
        reconnect_initial_backoff=timedelta(milliseconds=100),
        reconnect_max_backoff=timedelta(seconds=10),
        reconnect_backoff_jitter=0.5,
    )
    rng = random.Random(42)
    delays = [compute_backoff(a, opt, rng=rng).total_seconds()
              for a in range(1, 20)]
    # With ±50% jitter, no delay should equal the pure-exponential value
    # exactly, but all should be within [0.5*base, 1.5*base] of it.
    for i, d in enumerate(delays, start=1):
        base = 0.1 * (2 ** (i - 1))
        base = min(10.0, base)
        assert 0.5 * base <= d <= 1.5 * base


# ---------------------- keepalive helper ------------------------------


def test_configure_keepalive_doesnt_raise_on_fresh_socket():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        configure_keepalive(
            s,
            idle=timedelta(seconds=30),
            interval=timedelta(seconds=10),
            count=3,
        )
        # Verify SO_KEEPALIVE at least was set.
        val = s.getsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE)
        assert val != 0
    finally:
        s.close()


# --------- 1. offline buffer disabled → send during outage raises -----


async def test_offline_buffer_disabled_send_during_outage_raises():
    async def handler(reader, writer):
        await _drain_one_frame(reader)
        # Immediately close to simulate peer drop.

    srv = RestartableServer(handler)
    port = await srv.start()
    try:
        opt = ClientOptions(
            auto_reconnect=True,
            offline_buffer_capacity=0,   # explicit: no buffering
        )
        async with await Client.connect("127.0.0.1", port, opt) as c:
            await c.send(Transform())
            # Wait for peer-close to surface.
            await asyncio.sleep(0.2)
            with pytest.raises(ConnectionClosedError):
                for _ in range(5):
                    await c.send(Transform())
                    await asyncio.sleep(0.05)
    finally:
        await srv.stop()


# --------- 2. DROP_OLDEST preserves send during long outage -----------


async def test_drop_oldest_preserves_send():
    """Server closes immediately, never restarts; buffer fills and rotates."""
    async def handler(reader, writer):
        # Read exactly one message and then close. Reconnect attempts
        # will also land here and close immediately — so the buffer
        # keeps filling across attempts.
        try:
            await _drain_one_frame(reader)
        except Exception:
            pass

    srv = RestartableServer(handler)
    port = await srv.start()
    try:
        opt = ClientOptions(
            auto_reconnect=True,
            offline_buffer_capacity=3,
            offline_overflow_policy=OfflineOverflow.DROP_OLDEST,
            reconnect_initial_backoff=timedelta(milliseconds=500),
            reconnect_max_backoff=timedelta(seconds=10),
        )
        async with await Client.connect("127.0.0.1", port, opt) as c:
            await c.send(Transform())
            # Let peer-close surface.
            await asyncio.sleep(0.3)
            # All 10 sends succeed under DROP_OLDEST — the buffer
            # rotates its head as new messages arrive.
            for i in range(10):
                await c.send(Transform(
                    matrix=[1,0,0,0,1,0,0,0,1, float(i), 0, 0]
                ))
    finally:
        await srv.stop()


# --------- 3. DROP_NEWEST surfaces overflow ---------------------------


async def test_drop_newest_raises_overflow():
    """DROP_NEWEST buffers until full, then raises.

    We stop the server entirely before the overflow sends so writes
    actually fail (peer-close on an idle TCP connection without a
    running receiver isn't detected immediately — kernel buffers
    the write).
    """
    async def handler(reader, writer):
        await _drain_one_frame(reader)

    srv = RestartableServer(handler)
    port = await srv.start()

    opt = ClientOptions(
        auto_reconnect=True,
        offline_buffer_capacity=2,
        offline_overflow_policy=OfflineOverflow.DROP_NEWEST,
        # Long enough that no reconnect drain empties the buffer
        # mid-test; there's no server to reconnect to anyway.
        reconnect_initial_backoff=timedelta(seconds=5),
    )
    try:
        async with await Client.connect("127.0.0.1", port, opt) as c:
            await c.send(Transform())
            # Stop the server so subsequent writes RST.
            await srv.stop()
            await asyncio.sleep(0.1)

            # Force drop detection: first send after server-down
            # must fail the underlying write, which transitions the
            # client to the disconnected/buffering state.
            # It may need a retry or two to surface the RST.
            sent_disconnected = 0
            overflow_seen = False
            for _ in range(6):
                try:
                    await c.send(Transform())
                    sent_disconnected += 1
                except BufferOverflowError:
                    overflow_seen = True
                    break
                await asyncio.sleep(0.05)

            assert overflow_seen, (
                f"expected BufferOverflowError after "
                f"{sent_disconnected} buffered sends"
            )
    finally:
        await srv.stop()


# --------- 4. reconnect_max_attempts exhaustion → terminal ------------


async def test_reconnect_max_attempts_exhausts_to_terminal():
    async def handler(reader, writer):
        try:
            await _drain_one_frame(reader)
        except Exception:
            pass

    srv = RestartableServer(handler)
    port = await srv.start()
    try:
        opt = ClientOptions(
            auto_reconnect=True,
            offline_buffer_capacity=10,
            reconnect_initial_backoff=timedelta(milliseconds=50),
            reconnect_max_backoff=timedelta(milliseconds=100),
            reconnect_max_attempts=2,
            reconnect_backoff_jitter=0.0,
        )
        fails: list[tuple[int, timedelta]] = []
        async with await Client.connect("127.0.0.1", port, opt) as c:
            c.on_reconnect_failed(
                lambda att, delay: fails.append((att, delay))
            )

            await c.send(Transform())
            # Stop the server so reconnects fail.
            await srv.stop()
            await asyncio.sleep(0.1)

            # Force drop detection by triggering a send. On macOS/Linux
            # an idle TCP connection won't notice peer-close until
            # either a read returns 0 or a write hits RST.
            for _ in range(6):
                try:
                    await c.send(Transform())
                except (ConnectionClosedError, BufferOverflowError):
                    break
                await asyncio.sleep(0.05)

            # Wait for the reconnect task to exhaust.
            for _ in range(40):
                await asyncio.sleep(0.1)
                if c._terminal:    # noqa: SLF001 (test internal)
                    break

            assert c._terminal, "expected terminal after max_attempts"
            with pytest.raises(ConnectionClosedError):
                await c.send(Transform())
            assert len(fails) >= 2
    finally:
        await srv.stop()


# --------- happy-path auto-reconnect (buffer drains on reconnect) -----


async def test_auto_reconnect_drains_buffer_on_reconnect():
    """Server goes down for a beat then comes back; buffered messages arrive."""
    received: list[bytes] = []
    serve_more = asyncio.Event()

    async def handler(reader, writer):
        try:
            while True:
                type_id, body = await _drain_one_frame(reader)
                received.append(body)
        except Exception:
            pass

    srv = RestartableServer(handler)
    port = await srv.start()
    try:
        opt = ClientOptions(
            auto_reconnect=True,
            offline_buffer_capacity=10,
            offline_overflow_policy=OfflineOverflow.DROP_OLDEST,
            reconnect_initial_backoff=timedelta(milliseconds=50),
            reconnect_max_backoff=timedelta(milliseconds=200),
        )
        connected_events: list[str] = []
        disconnected_events: list[str] = []

        async with await Client.connect("127.0.0.1", port, opt) as c:
            c.on_connected(lambda: connected_events.append("up"))
            c.on_disconnected(
                lambda exc: disconnected_events.append("down"),
            )

            await c.send(Transform(matrix=[1,0,0,0,1,0,0,0,1,1,0,0]))
            await asyncio.sleep(0.2)

            # Take the server down.
            await srv.stop()
            await asyncio.sleep(0.2)

            # Send while disconnected — goes to the buffer.
            await c.send(Transform(matrix=[1,0,0,0,1,0,0,0,1,2,0,0]))
            await c.send(Transform(matrix=[1,0,0,0,1,0,0,0,1,3,0,0]))

            # Bring the server back on the same port.
            await srv.start(port=port)

            # Give the reconnect worker a generous budget.
            for _ in range(50):
                await asyncio.sleep(0.1)
                if len(received) >= 3:
                    break

            assert len(received) >= 3, (
                f"expected at least 3 messages, got {len(received)}"
            )
            assert len(connected_events) >= 1
            assert len(disconnected_events) >= 1
    finally:
        await srv.stop()
