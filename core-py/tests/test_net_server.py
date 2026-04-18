"""End-to-end tests for the async :class:`~oigtl.net.Server`.

Phase 5 covers the happy path: listen, accept, dispatch, serve,
close. Restriction tests land in Phase 6.

Client-side we reuse :class:`~oigtl.net.Client` from Phase 2 so
the round trip is real.
"""

from __future__ import annotations

import asyncio
from datetime import timedelta

import pytest

from oigtl.messages import Status, Transform
from oigtl.net import Client, Server, ServerOptions


async def test_listen_returns_port_zero_resolves():
    """listen(0) picks a free port; the property reports the real one."""
    server = await Server.listen(0)
    try:
        assert server.port > 0
    finally:
        await server.close()


async def test_single_client_round_trip():
    """Client sends TRANSFORM; server replies with STATUS; both close cleanly."""
    server = await Server.listen(0)

    @server.on(Transform)
    async def _(env, peer):
        await peer.send(Status(
            code=1, sub_code=0, error_name="",
            status_message="acked",
        ))

    serve_task = asyncio.create_task(server.serve())
    try:
        async with await Client.connect("127.0.0.1", server.port) as c:
            await c.send(Transform(matrix=[1,0,0,0,1,0,0,0,1,0,0,0]))
            env = await c.receive(Status, timeout=2)
            assert env.body.status_message == "acked"
    finally:
        await server.close()
        await serve_task


async def test_multiple_peers_concurrent():
    """Two clients connected simultaneously get independent dispatch."""
    received: dict[str, list[str]] = {}

    server = await Server.listen(0)

    @server.on(Transform)
    async def _(env, peer):
        tag = f"{peer.address.address}:{peer.address.port}"
        received.setdefault(tag, []).append(
            f"{env.body.matrix[-3:]}"
        )

    serve_task = asyncio.create_task(server.serve())
    try:
        c1 = await Client.connect("127.0.0.1", server.port)
        c2 = await Client.connect("127.0.0.1", server.port)
        try:
            await c1.send(Transform(
                matrix=[1,0,0,0,1,0,0,0,1, 1,0,0],
            ))
            await c2.send(Transform(
                matrix=[1,0,0,0,1,0,0,0,1, 2,0,0],
            ))
            # Give the server a tick to dispatch.
            for _ in range(20):
                await asyncio.sleep(0.05)
                if len(received) >= 2:
                    break
            assert len(received) == 2, received
        finally:
            await c1.close()
            await c2.close()
    finally:
        await server.close()
        await serve_task


async def test_on_connected_and_disconnected_callbacks():
    connected: list[str] = []
    disconnected: list[str] = []

    server = await Server.listen(0)

    @server.on_connected
    def _(peer):
        connected.append(str(peer.address))

    @server.on_disconnected
    def _(peer, cause):
        disconnected.append(str(peer.address))

    serve_task = asyncio.create_task(server.serve())
    try:
        c = await Client.connect("127.0.0.1", server.port)
        await asyncio.sleep(0.1)
        await c.close()
        # Give the server a beat to observe the FIN.
        for _ in range(20):
            await asyncio.sleep(0.05)
            if disconnected:
                break

        assert len(connected) == 1
        assert len(disconnected) == 1
        assert connected[0] == disconnected[0]
    finally:
        await server.close()
        await serve_task


async def test_unknown_type_routed_to_on_unknown():
    """Message types with no typed registration fall through to on_unknown."""
    received: list[str] = []

    server = await Server.listen(0)

    @server.on_unknown
    async def _(env, peer):
        received.append(env.header.type_id)

    serve_task = asyncio.create_task(server.serve())
    try:
        # Send a TRANSFORM without registering an on(Transform) —
        # it's in the REGISTRY so it decodes typed, but if we
        # registered nothing, it falls to on_unknown through the
        # "no typed handler" branch. Actually — wait: TRANSFORM *is*
        # typed. on_unknown only fires for wire type_ids absent from
        # the registry OR for typed messages with no on() handler.
        # Our server routes unknown-to-handler-dispatch through the
        # same on_unknown. Confirm via an un-handled but known type.
        async with await Client.connect(
            "127.0.0.1", server.port,
        ) as c:
            await c.send(Transform())
            for _ in range(20):
                await asyncio.sleep(0.05)
                if received:
                    break
        assert received == ["TRANSFORM"]
    finally:
        await server.close()
        await serve_task


async def test_max_message_size_closes_peer():
    """Exceeding max_message_size framing cap disconnects the peer."""
    opt = ServerOptions(max_message_size=10)    # tiny cap
    server = await Server.listen(0, opt)

    disconnected = asyncio.Event()

    @server.on_disconnected
    def _(peer, cause):
        disconnected.set()

    serve_task = asyncio.create_task(server.serve())
    try:
        async with await Client.connect(
            "127.0.0.1", server.port,
        ) as c:
            # Transform body is 48 bytes > 10 cap.
            try:
                await c.send(Transform())
            except Exception:
                pass
            # Server should close the peer for the oversize body.
            await asyncio.wait_for(disconnected.wait(), timeout=2)
    finally:
        await server.close()
        await serve_task


async def test_context_manager_closes_cleanly():
    server = await Server.listen(0)
    async with server:
        assert server.port > 0
    # After __aexit__, serve() returns immediately.
    await asyncio.wait_for(server.serve(), timeout=1)


async def test_server_close_disconnects_active_peers():
    server = await Server.listen(0)

    @server.on(Transform)
    async def _(env, peer):
        pass

    serve_task = asyncio.create_task(server.serve())
    try:
        c = await Client.connect("127.0.0.1", server.port)
        await c.send(Transform())
        await asyncio.sleep(0.1)
        assert len(server.peers) == 1

        await server.close()
        # Client should see its connection end.
        await asyncio.wait_for(
            _wait_peer_close(c), timeout=2,
        )
    finally:
        await c.close()
        await serve_task


async def _wait_peer_close(client):
    """Helper: wait for a client to see the server go away."""
    from oigtl.net.errors import ConnectionClosedError
    # Any receive will surface the EOF.
    try:
        await client.receive(Status, timeout=1)
    except ConnectionClosedError:
        return
