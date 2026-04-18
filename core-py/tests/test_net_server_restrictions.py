"""Server restriction tests — Phase 6.

Exercises the security-motivated subset: peer-address filtering,
max-clients cap, idle timeout, max-message-size cap, and the
fluent builders (``allow``, ``restrict_to_local_subnet``,
``restrict_to_this_machine_only``, ``set_max_clients``,
``disconnect_if_silent_for``, ``set_max_message_size_bytes``).

These are what the whole ``oigtl.net`` exists to provide — a
researcher should be able to stand up a tracker server that
accepts only their lab LAN in one line.
"""

from __future__ import annotations

import asyncio
import ipaddress
from datetime import timedelta

import pytest

from oigtl.messages import Status, Transform
from oigtl.net import Client, Server, interfaces
from oigtl.net.errors import ConnectionClosedError
from oigtl.net.policy import PeerPolicy, parse


# ------------------------- peer-address filter ------------------------


async def test_allow_admits_listed_peer():
    """Client on loopback matches an allow-list that includes loopback."""
    server = (await Server.listen(0)).allow("127.0.0.0/8")

    @server.on(Transform)
    async def _(env, peer):
        await peer.send(Status(
            code=1, sub_code=0, error_name="", status_message="ok",
        ))

    serve_task = asyncio.create_task(server.serve())
    try:
        async with await Client.connect(
            "127.0.0.1", server.port,
        ) as c:
            await c.send(Transform())
            env = await c.receive(Status, timeout=2)
            assert env.body.status_message == "ok"
    finally:
        await server.close()
        await serve_task


async def test_allow_rejects_unlisted_peer():
    """Allow-list that excludes loopback rejects the test client."""
    # A network we're definitely not on.
    server = (await Server.listen(0)).allow("10.42.0.0/24")

    connected: list = []

    @server.on_connected
    def _(peer):
        connected.append(peer)

    serve_task = asyncio.create_task(server.serve())
    try:
        c = await Client.connect("127.0.0.1", server.port)
        try:
            # Server accepts TCP then immediately closes us out.
            # Any read will see EOF.
            with pytest.raises(ConnectionClosedError):
                await c.receive(Status, timeout=1)
        finally:
            await c.close()

        # on_connected shouldn't have fired — rejected before admit.
        assert connected == []
    finally:
        await server.close()
        await serve_task


async def test_allow_accepts_list_of_networks():
    """``allow([net1, net2, ...])`` composes additively."""
    server = (await Server.listen(0)).allow([
        "10.42.0.0/24",            # we're not on this
        "127.0.0.0/8",             # we ARE on this
    ])

    @server.on(Transform)
    async def _(env, peer):
        await peer.send(Status(
            code=1, sub_code=0, error_name="", status_message="ok",
        ))

    serve_task = asyncio.create_task(server.serve())
    try:
        async with await Client.connect(
            "127.0.0.1", server.port,
        ) as c:
            await c.send(Transform())
            env = await c.receive(Status, timeout=2)
            assert env.body.status_message == "ok"
    finally:
        await server.close()
        await serve_task


async def test_allow_accepts_stdlib_network_objects():
    """The researcher idiom: feed interfaces.subnets() straight in."""
    server = (await Server.listen(0)).allow(
        interfaces.subnets(include_loopback=True),
    )
    assert server.options.policy is not None
    # Every configured range should be an IpRange.
    assert server.options.policy.allowed_peers
    await server.close()


async def test_allow_rejects_garbage_input():
    server = await Server.listen(0)
    try:
        with pytest.raises(ValueError):
            server.allow("not an ip range")
    finally:
        await server.close()


async def test_restrict_to_this_machine_only_roundtrip():
    """Builder shortcut — loopback can connect."""
    server = (await Server.listen(0)).restrict_to_this_machine_only()

    @server.on(Transform)
    async def _(env, peer):
        await peer.send(Status(
            code=1, sub_code=0, error_name="", status_message="ok",
        ))

    serve_task = asyncio.create_task(server.serve())
    try:
        async with await Client.connect(
            "127.0.0.1", server.port,
        ) as c:
            await c.send(Transform())
            env = await c.receive(Status, timeout=2)
            assert env.body.status_message == "ok"
    finally:
        await server.close()
        await serve_task


# --------------------------- max_clients ------------------------------


async def test_set_max_clients_rejects_over_cap():
    server = (await Server.listen(0)).set_max_clients(1)

    # A handler that blocks so peer #1 stays connected.
    hold = asyncio.Event()

    @server.on(Transform)
    async def _(env, peer):
        await hold.wait()

    serve_task = asyncio.create_task(server.serve())
    try:
        c1 = await Client.connect("127.0.0.1", server.port)
        try:
            await c1.send(Transform())
            await asyncio.sleep(0.1)    # let peer #1 settle
            assert len(server.peers) == 1

            # Peer #2 — server should reject at admit time.
            c2 = await Client.connect("127.0.0.1", server.port)
            try:
                with pytest.raises(ConnectionClosedError):
                    await c2.receive(Status, timeout=1)
            finally:
                await c2.close()
        finally:
            hold.set()
            await c1.close()
    finally:
        await server.close()
        await serve_task


# --------------------------- idle timeout -----------------------------


async def test_disconnect_if_silent_for_closes_idle_peer():
    server = (await Server.listen(0)).disconnect_if_silent_for(
        timedelta(milliseconds=200),
    )

    disconnected = asyncio.Event()

    @server.on_disconnected
    def _(peer, cause):
        disconnected.set()

    serve_task = asyncio.create_task(server.serve())
    try:
        async with await Client.connect(
            "127.0.0.1", server.port,
        ) as c:
            # Do nothing. Server should kick us in ~200 ms.
            await asyncio.wait_for(disconnected.wait(), timeout=2)
    finally:
        await server.close()
        await serve_task


async def test_disconnect_if_silent_for_accepts_int_ms():
    """Builder accepts int ms (ClientOptions convention)."""
    server = (await Server.listen(0)).disconnect_if_silent_for(500)
    assert server.options.idle_timeout_seconds == 0.5
    await server.close()


async def test_disconnect_if_silent_for_accepts_float_seconds():
    server = (await Server.listen(0)).disconnect_if_silent_for(1.5)
    assert server.options.idle_timeout_seconds == 1.5
    await server.close()


# --------------------------- max message size -------------------------


async def test_set_max_message_size_bytes_builder():
    server = (await Server.listen(0)).set_max_message_size_bytes(10)

    disconnected = asyncio.Event()

    @server.on_disconnected
    def _(peer, cause):
        disconnected.set()

    serve_task = asyncio.create_task(server.serve())
    try:
        async with await Client.connect(
            "127.0.0.1", server.port,
        ) as c:
            # Transform body is 48 > 10 cap.
            try:
                await c.send(Transform())
            except Exception:
                pass
            await asyncio.wait_for(disconnected.wait(), timeout=2)
    finally:
        await server.close()
        await serve_task


# --------------------------- builder chaining -------------------------


async def test_builders_chain_and_return_self():
    server = await Server.listen(0)
    result = (
        server
        .allow("127.0.0.0/8")
        .set_max_clients(4)
        .disconnect_if_silent_for(1.0)
        .set_max_message_size_bytes(65536)
    )
    assert result is server
    assert server.options.max_clients == 4
    assert server.options.idle_timeout_seconds == 1.0
    assert server.options.max_message_size == 65536
    assert server.options.policy is not None
    assert len(server.options.policy.allowed_peers) == 1
    await server.close()


# --------------------------- policy integration -----------------------


def test_peer_policy_direct_config_also_works():
    """Callers may set options.policy directly rather than via builders."""
    policy = PeerPolicy(allowed_peers=[parse("127.0.0.0/8")])  # type: ignore[list-item]
    assert policy.is_peer_allowed("127.0.0.1")
    assert not policy.is_peer_allowed("10.0.0.1")
