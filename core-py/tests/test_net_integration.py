"""End-to-end integration: Client ↔ Server through the full stack.

These tests exercise the researcher-facing flow a user is likely
to write — not mocked pieces. Stands up a real :class:`Server`
with restrictions, connects a real :class:`Client`, sends the
stress-test message set, and checks both sides see what they
should.
"""

from __future__ import annotations

import asyncio
from datetime import timedelta

import pytest

from oigtl.messages import (
    Point,
    Position,
    Sensor,
    Status,
    String,
    Transform,
)
from oigtl.net import (
    Client,
    ClientOptions,
    OfflineOverflow,
    Server,
    interfaces,
)


async def test_stress_test_message_set_round_trips():
    """The 6 message types we care most about all survive a round trip."""
    received: list = []
    server = (await Server.listen(0)).restrict_to_this_machine_only()

    for typ in (Transform, Status, String, Position, Point, Sensor):
        @server.on(typ)
        async def _(env, peer, _typ=typ):
            received.append((_typ.__name__, env.body))

    serve_task = asyncio.create_task(server.serve())
    try:
        async with await Client.connect(
            "127.0.0.1", server.port,
        ) as c:
            await c.send(Transform(
                matrix=[1,0,0,0,1,0,0,0,1, 1,2,3],
            ))
            await c.send(Status(
                code=1, sub_code=0, error_name="", status_message="ok",
            ))
            await c.send(String(string="hello"))
            await c.send(Position(
                position=[1.0, 2.0, 3.0],
                quaternion=[0.0, 0.0, 0.0, 1.0],
            ))
            await c.send(Point(point_elements=[]))
            await c.send(Sensor())

            for _ in range(40):
                await asyncio.sleep(0.05)
                if len(received) >= 6:
                    break
            assert len(received) == 6, received
    finally:
        await server.close()
        await serve_task


async def test_restricted_lan_server_with_real_lan_peer():
    """The target researcher use case: LAN-only server, local client connects."""
    server = (await Server.listen(0)).restrict_to_local_subnet()

    @server.on(Transform)
    async def _(env, peer):
        await peer.send(Status(
            code=1, sub_code=0, error_name="",
            status_message=f"hello {peer.address.address}",
        ))

    serve_task = asyncio.create_task(server.serve())
    try:
        # Connect to our primary address — which is on a subnet we
        # allow because it was enumerated by interfaces.subnets().
        primary = interfaces.primary_address()
        if primary is None:
            pytest.skip("no non-loopback address; skipping LAN test")
        async with await Client.connect(
            str(primary), server.port,
        ) as c:
            await c.send(Transform())
            env = await c.receive(Status, timeout=2)
            assert env.body.status_message.startswith("hello ")
    finally:
        await server.close()
        await serve_task


async def test_client_resilience_survives_server_restart_on_real_server():
    """Full resilience story: Server-A-close → reconnect to Server-B → buffered drain."""
    server_a = await Server.listen(0)
    port = server_a.port

    received_a: list[str] = []
    received_b: list[str] = []

    @server_a.on(Transform)
    async def _(env, peer):
        received_a.append(str(env.body.matrix[-3:]))

    serve_a = asyncio.create_task(server_a.serve())

    opt = ClientOptions(
        auto_reconnect=True,
        offline_buffer_capacity=20,
        offline_overflow_policy=OfflineOverflow.DROP_OLDEST,
        reconnect_initial_backoff=timedelta(milliseconds=50),
        reconnect_max_backoff=timedelta(milliseconds=200),
    )
    async with await Client.connect("127.0.0.1", port, opt) as c:
        await c.send(Transform(
            matrix=[1,0,0,0,1,0,0,0,1, 1, 0, 0],
        ))
        await asyncio.sleep(0.2)

        # Take server A down.
        await server_a.close()
        await serve_a
        # Give the client's monitor a tick to notice the FIN.
        # Sends made immediately after server close would otherwise
        # race the drop detection and hit the live writer (where
        # the kernel would buffer then discard the bytes once the
        # peer has gone).
        await asyncio.sleep(0.2)

        # Send during outage — goes to the buffer.
        for i in range(2, 5):
            await c.send(Transform(
                matrix=[1,0,0,0,1,0,0,0,1, float(i), 0, 0],
            ))

        # Bring up server B on the same port.
        server_b = await Server.listen(port)

        @server_b.on(Transform)
        async def _(env, peer):
            received_b.append(str(env.body.matrix[-3:]))

        serve_b = asyncio.create_task(server_b.serve())
        try:
            # Let the reconnect + drain run.
            for _ in range(50):
                await asyncio.sleep(0.1)
                if len(received_b) >= 3:
                    break

            assert len(received_a) == 1, received_a
            # Drain delivered at least the buffered items that made
            # it past the TCP drop race (some may have been written
            # to the pre-FIN kernel buffer).
            assert len(received_b) >= 2, (
                f"expected buffered messages to drain; got {received_b}"
            )
        finally:
            await server_b.close()
            await serve_b


async def test_server_rejects_over_max_clients():
    """Researcher scenario: cap clients, third arrival gets a clean rejection."""
    server = (await Server.listen(0)).set_max_clients(2)

    hold = asyncio.Event()

    @server.on(Transform)
    async def _(env, peer):
        await hold.wait()

    serve_task = asyncio.create_task(server.serve())
    try:
        c1 = await Client.connect("127.0.0.1", server.port)
        c2 = await Client.connect("127.0.0.1", server.port)
        try:
            await c1.send(Transform())
            await c2.send(Transform())
            await asyncio.sleep(0.1)
            assert len(server.peers) == 2

            c3 = await Client.connect("127.0.0.1", server.port)
            try:
                # Third client gets rejected — receive surfaces EOF.
                from oigtl.net.errors import ConnectionClosedError
                with pytest.raises(ConnectionClosedError):
                    await c3.receive(Status, timeout=1)
            finally:
                await c3.close()
        finally:
            hold.set()
            await c1.close()
            await c2.close()
    finally:
        await server.close()
        await serve_task
