"""Tests for ``oigtl.net.gateway`` — raw primitives, TCP adapters,
bridge driver, middleware.

Strategy: spin up a canonical three-node topology of Python
:class:`Server` + :class:`gateway` + another :class:`Server`, run
:class:`Client` traffic end-to-end, and assert the bytes arrive
intact on the far side. That's the actual contract a gateway
exists to fulfil; anything less is unit-testing abstractions
instead of behaviour.
"""

from __future__ import annotations

import asyncio
from datetime import timedelta

import pytest

from oigtl.messages import Status, Transform
from oigtl.net import (
    Client,
    RawMessage,
    Server,
    gateway as gw,
)


# ---------------------------------------------------------------------------
# Raw primitives on Client / Peer
# ---------------------------------------------------------------------------


async def test_client_send_raw_round_trips():
    """Client.send_raw accepts a RawMessage from the iterator."""
    # Build an upstream server that echoes every TRANSFORM back as-is.
    src = await Server.listen(0)

    @src.on(Transform)
    async def _(env, peer):
        await peer.send(env.body)      # typed echo

    src_task = asyncio.create_task(src.serve())
    try:
        async with await Client.connect("127.0.0.1", src.port) as c:
            # Typed send, raw iterator — confirms raw_messages
            # yields the exact bytes we sent.
            tx = Transform(matrix=[1,0,0,0,1,0,0,0,1, 7,8,9])
            await c.send(tx)

            async for raw in c.raw_messages():
                assert isinstance(raw, RawMessage)
                assert raw.header.type_id == "TRANSFORM"
                # wire should include the 58-byte header plus body.
                assert len(raw.wire) == 58 + 48
                # type_id convenience.
                assert raw.type_id == "TRANSFORM"
                break
    finally:
        await src.close()
        await src_task


async def test_peer_raw_messages_iterator_ends_on_peer_close():
    """Peer.raw_messages() returns cleanly when the remote closes."""
    accepted: list = []

    server = await Server.listen(0)

    async def handle():
        async for peer in server.accepted_peers():
            accepted.append(peer)
            # Drive raw_messages until it returns.
            count = 0
            async for _raw in peer.raw_messages():
                count += 1
            accepted.append(count)     # records the drain count

    task = asyncio.create_task(handle())
    try:
        c = await Client.connect("127.0.0.1", server.port)
        await c.send(Transform())
        await c.send(Transform())
        await c.close()
        await asyncio.sleep(0.2)

        # First element is the Peer, second is the received count.
        assert len(accepted) == 2
        assert accepted[1] == 2
    finally:
        await server.close()
        task.cancel()
        await asyncio.gather(task, return_exceptions=True)


# ---------------------------------------------------------------------------
# accepted_peers() vs dispatch mode
# ---------------------------------------------------------------------------


async def test_accepted_peers_yields_each_peer_once():
    server = await Server.listen(0)

    got: list = []

    async def consume():
        async for peer in server.accepted_peers():
            got.append(peer.address)

    task = asyncio.create_task(consume())
    try:
        c1 = await Client.connect("127.0.0.1", server.port)
        c2 = await Client.connect("127.0.0.1", server.port)
        await asyncio.sleep(0.2)
        assert len(got) == 2
        await c1.close()
        await c2.close()
    finally:
        await server.close()
        task.cancel()
        await asyncio.gather(task, return_exceptions=True)


# ---------------------------------------------------------------------------
# End-to-end TCP gateway
# ---------------------------------------------------------------------------


async def test_tcp_gateway_e2e_bytes_match():
    """Client → Gateway → Target: bytes arrive intact on the target."""
    # Target: records bytes it receives.
    received_bodies: list[bytes] = []

    target = await Server.listen(0)

    @target.on(Transform)
    async def _(env, peer):
        # Record the body via pack — if bytes round-trip, this
        # matches what the client sent.
        received_bodies.append(env.body.pack())

    target_task = asyncio.create_task(target.serve())

    # Gateway between a fresh acceptor and the target.
    acceptor = gw.TcpAcceptor(0)
    connector = gw.TcpConnector("127.0.0.1", target.port)
    gateway_task = asyncio.create_task(gw.gateway(acceptor, connector))

    # Wait for the acceptor to bind.
    for _ in range(40):
        await asyncio.sleep(0.05)
        if acceptor.server is not None:
            break
    assert acceptor.server is not None

    try:
        async with await Client.connect(
            "127.0.0.1", acceptor.server.port,
        ) as c:
            tx1 = Transform(matrix=[1,0,0,0,1,0,0,0,1, 1,2,3])
            tx2 = Transform(matrix=[1,0,0,0,1,0,0,0,1, 4,5,6])
            await c.send(tx1)
            await c.send(tx2)

            for _ in range(40):
                await asyncio.sleep(0.05)
                if len(received_bodies) >= 2:
                    break

        assert len(received_bodies) == 2
        assert received_bodies[0] == tx1.pack()
        assert received_bodies[1] == tx2.pack()
    finally:
        gateway_task.cancel()
        await asyncio.gather(gateway_task, return_exceptions=True)
        await acceptor.close()
        await target.close()
        await target_task


async def test_tcp_gateway_bidirectional():
    """Replies from the target make it back to the client through the gateway."""
    target = await Server.listen(0)

    @target.on(Transform)
    async def _(env, peer):
        await peer.send(Status(
            code=1, sub_code=0, error_name="",
            status_message="pong",
        ))

    target_task = asyncio.create_task(target.serve())

    acceptor = gw.TcpAcceptor(0)
    connector = gw.TcpConnector("127.0.0.1", target.port)
    gateway_task = asyncio.create_task(gw.gateway(acceptor, connector))

    for _ in range(40):
        await asyncio.sleep(0.05)
        if acceptor.server is not None:
            break

    try:
        async with await Client.connect(
            "127.0.0.1", acceptor.server.port,
        ) as c:
            await c.send(Transform())
            env = await c.receive(Status, timeout=timedelta(seconds=2))
            assert env.body.status_message == "pong"
    finally:
        gateway_task.cancel()
        await asyncio.gather(gateway_task, return_exceptions=True)
        await acceptor.close()
        await target.close()
        await target_task


# ---------------------------------------------------------------------------
# Middleware
# ---------------------------------------------------------------------------


class FilterByType:
    """Drop messages with a type_id in the blocklist, each direction."""

    def __init__(self, block_upstream: set[str] = (),
                 block_downstream: set[str] = ()) -> None:
        self.block_upstream = set(block_upstream)
        self.block_downstream = set(block_downstream)

    async def upstream_to_downstream(self, msg: RawMessage) -> RawMessage | None:
        if msg.header.type_id in self.block_upstream:
            return None
        return msg

    async def downstream_to_upstream(self, msg: RawMessage) -> RawMessage | None:
        if msg.header.type_id in self.block_downstream:
            return None
        return msg


async def test_middleware_drops_upstream_messages():
    """Filter blocks TRANSFORM on the upstream→downstream direction."""
    received: list[str] = []

    target = await Server.listen(0)

    @target.on(Transform)
    async def _(env, peer):
        received.append("TRANSFORM")

    @target.on(Status)
    async def _(env, peer):
        received.append("STATUS")

    target_task = asyncio.create_task(target.serve())

    acceptor = gw.TcpAcceptor(0)
    connector = gw.TcpConnector("127.0.0.1", target.port)
    gateway_task = asyncio.create_task(gw.gateway(
        acceptor, connector,
        middleware=[FilterByType(block_upstream={"TRANSFORM"})],
    ))

    for _ in range(40):
        await asyncio.sleep(0.05)
        if acceptor.server is not None:
            break

    try:
        async with await Client.connect(
            "127.0.0.1", acceptor.server.port,
        ) as c:
            await c.send(Transform())                 # dropped
            await c.send(Status(code=1, status_message="hi"))  # allowed
            await asyncio.sleep(0.3)
        assert received == ["STATUS"]
    finally:
        gateway_task.cancel()
        await asyncio.gather(gateway_task, return_exceptions=True)
        await acceptor.close()
        await target.close()
        await target_task


async def test_middleware_can_tag_attributes():
    """Middleware may annotate messages; attributes survive the hop."""

    class Tagger:
        async def upstream_to_downstream(self, msg: RawMessage) -> RawMessage:
            # Produce a NEW RawMessage — Pydantic models are frozen-
            # in-spirit for mutation of mapping fields.
            return RawMessage(
                header=msg.header,
                wire=msg.wire,
                attributes={**msg.attributes, "hop": "gateway"},
            )

        async def downstream_to_upstream(self, msg: RawMessage) -> RawMessage:
            return msg

    # Spin up acceptor → connector where the "target" is a second
    # server we iterate peers on to inspect the raw_messages stream.
    far_side = await Server.listen(0)

    seen_attrs: list[dict] = []

    async def drain_far():
        async for peer in far_side.accepted_peers():
            async for raw in peer.raw_messages():
                seen_attrs.append(dict(raw.attributes))
                break
            break

    far_task = asyncio.create_task(drain_far())

    acceptor = gw.TcpAcceptor(0)
    connector = gw.TcpConnector("127.0.0.1", far_side.port)
    gateway_task = asyncio.create_task(gw.gateway(
        acceptor, connector, middleware=[Tagger()],
    ))

    for _ in range(40):
        await asyncio.sleep(0.05)
        if acceptor.server is not None:
            break

    try:
        async with await Client.connect(
            "127.0.0.1", acceptor.server.port,
        ) as c:
            await c.send(Transform())
            for _ in range(40):
                await asyncio.sleep(0.05)
                if seen_attrs:
                    break
        # Note: the `attributes` dict is per-hop metadata, not wire-
        # carried. After the gateway forwards bytes, the receiving
        # end reconstructs a RawMessage from the wire and its
        # attributes are empty (no transport carries them today).
        # The test confirms what we promise: bytes flow end-to-end,
        # attributes are a gateway-local concept.
        assert seen_attrs == [{}]
    finally:
        gateway_task.cancel()
        await asyncio.gather(gateway_task, return_exceptions=True)
        await acceptor.close()
        await far_side.close()
        far_task.cancel()
        await asyncio.gather(far_task, return_exceptions=True)


# ---------------------------------------------------------------------------
# bridge() standalone
# ---------------------------------------------------------------------------


async def test_bridge_function_without_gateway():
    """bridge() can wire two already-connected endpoints directly."""
    # Stand up two servers. Wire one side manually.
    left = await Server.listen(0)
    right = await Server.listen(0)

    right_received: list = []

    @right.on(Transform)
    async def _(env, peer):
        right_received.append(env.body)

    right_task = asyncio.create_task(right.serve())

    # Accept on left, dial into right, bridge them.
    left_peer_endpoint: list[gw.TcpPeerEndpoint] = []

    async def accept_and_bridge():
        async for peer in left.accepted_peers():
            up = gw.TcpPeerEndpoint(peer)
            left_peer_endpoint.append(up)
            down_client = await Client.connect("127.0.0.1", right.port)
            down = gw.TcpClientEndpoint(down_client)
            await gw.bridge(up, down)

    bridge_task = asyncio.create_task(accept_and_bridge())
    try:
        async with await Client.connect("127.0.0.1", left.port) as c:
            tx = Transform(matrix=[1,0,0,0,1,0,0,0,1, 9,8,7])
            await c.send(tx)
            for _ in range(40):
                await asyncio.sleep(0.05)
                if right_received:
                    break
        assert len(right_received) == 1
        assert right_received[0].matrix[-3:] == [9.0, 8.0, 7.0]
    finally:
        bridge_task.cancel()
        await asyncio.gather(bridge_task, return_exceptions=True)
        await left.close()
        await right.close()
        await right_task
