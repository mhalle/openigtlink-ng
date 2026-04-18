"""Tests for :class:`~oigtl.net.SyncServer`.

Delegation wrapper — we test that the sync surface works and that
restriction builders chain correctly. The deeper behaviour is
already covered by the async server tests.

This test file must stay sync. We drive it from a separate thread
so ``serve()`` can block without wedging the test.
"""

from __future__ import annotations

import threading
import time
from datetime import timedelta

import pytest

from oigtl.messages import Status, Transform
from oigtl.net import Server, SyncClient, SyncServer


def test_sync_server_builders_chain():
    server = (
        SyncServer.listen(0)
        .restrict_to_this_machine_only()
        .set_max_clients(4)
        .disconnect_if_silent_for(1.0)
    )
    try:
        assert server.port > 0
        assert server.options.max_clients == 4
        assert server.options.idle_timeout_seconds == 1.0
        assert server.options.policy is not None
    finally:
        server.close()


def test_sync_server_round_trip():
    """Sync server + sync client + async handler → round-trip status."""
    server = SyncServer.listen(0).restrict_to_this_machine_only()

    @server.on(Transform)
    async def _(env, peer):
        await peer.send(Status(
            code=1, sub_code=0, error_name="",
            status_message="sync-ack",
        ))

    def run_server():
        server.serve()

    thread = threading.Thread(target=run_server, daemon=True)
    thread.start()
    # Give the event loop a moment to get into serve().
    time.sleep(0.1)

    try:
        with SyncClient.connect("127.0.0.1", server.port) as c:
            c.send(Transform())
            env = c.receive(Status, timeout=2)
            assert env.body.status_message == "sync-ack"
    finally:
        server.close()
        thread.join(timeout=2)


def test_sync_server_context_manager_closes():
    """``with SyncServer.listen(...) as server:`` closes cleanly."""
    with SyncServer.listen(0).restrict_to_this_machine_only() as server:
        assert server.port > 0
    # After exit, peers is empty and serve() would return immediately.
    assert server.peers == frozenset()


def test_listen_sync_classmethod_on_async_server():
    """``Server.listen_sync()`` returns a SyncServer."""
    server = Server.listen_sync(0)
    try:
        assert isinstance(server, SyncServer)
        assert server.port > 0
    finally:
        server.close()
