"""Synchronous wrapper over :class:`~oigtl.net.server.Server`.

Same capability as the async server, blocking semantics, same
shared background event loop as :class:`~oigtl.net.SyncClient`.

    from oigtl.net import SyncServer, interfaces
    from oigtl.messages import Transform, Status

    server = SyncServer.listen(18944).restrict_to_local_subnet()

    @server.on(Transform)
    async def _(env, peer):           # handlers still async
        await peer.send(Status(code=1, ...))

    server.serve()                    # blocks until close()

Also importable as ``Server.listen_sync`` for symmetry with the
async API::

    from oigtl.net import Server
    server = Server.listen_sync(18944)

Handlers stay async because they run on the event loop — mixing
sync callbacks into asyncio is a footgun. If a researcher wants
pure-blocking handler code, they can use
``server.messages_for(peer)`` (future enhancement) or run their
work in ``asyncio.to_thread``.
"""

from __future__ import annotations

from datetime import timedelta
from typing import TYPE_CHECKING, TypeVar

from pydantic import BaseModel

from oigtl.net._event_loop import submit
from oigtl.net.server import Peer, Server as _AsyncServer, ServerOptions

if TYPE_CHECKING:
    import ipaddress

__all__ = ["SyncServer"]


M = TypeVar("M", bound=BaseModel)


class SyncServer:
    """Blocking façade over :class:`oigtl.net.server.Server`.

    Methods mirror the async server's names; each dispatches to the
    shared background loop. Thread-safe — the underlying loop
    serialises.
    """

    def __init__(self, inner: _AsyncServer) -> None:
        self._inner = inner

    # --------------------------------------------------------------
    # Construction
    # --------------------------------------------------------------

    @classmethod
    def listen(
        cls,
        port: int,
        options: ServerOptions | None = None,
        *,
        host: str = "0.0.0.0",
    ) -> "SyncServer":
        """Bind and start listening; don't accept yet. Blocking."""
        inner = submit(
            _AsyncServer.listen(port, options, host=host)
        ).result()
        return cls(inner)

    # --------------------------------------------------------------
    # Properties
    # --------------------------------------------------------------

    @property
    def options(self) -> ServerOptions:
        return self._inner.options

    @property
    def port(self) -> int:
        return self._inner.port

    @property
    def peers(self) -> frozenset[Peer]:
        return self._inner.peers

    # --------------------------------------------------------------
    # Restriction builders — delegate and return self for chaining.
    # --------------------------------------------------------------

    def allow(self, peers) -> "SyncServer":
        self._inner.allow(peers)
        return self

    def restrict_to_local_subnet(self) -> "SyncServer":
        self._inner.restrict_to_local_subnet()
        return self

    def restrict_to_this_machine_only(self) -> "SyncServer":
        self._inner.restrict_to_this_machine_only()
        return self

    def set_max_clients(self, n: int) -> "SyncServer":
        self._inner.set_max_clients(n)
        return self

    def disconnect_if_silent_for(
        self, timeout: timedelta | float | int,
    ) -> "SyncServer":
        self._inner.disconnect_if_silent_for(timeout)
        return self

    def set_max_message_size_bytes(self, n: int) -> "SyncServer":
        self._inner.set_max_message_size_bytes(n)
        return self

    # --------------------------------------------------------------
    # Handler registration (async handlers; the inner server runs
    # them on the background loop).
    # --------------------------------------------------------------

    def on(self, message_type: type[M]):
        return self._inner.on(message_type)

    def on_unknown(self, handler):
        return self._inner.on_unknown(handler)

    def on_connected(self, handler):
        return self._inner.on_connected(handler)

    def on_disconnected(self, handler):
        return self._inner.on_disconnected(handler)

    def on_error(self, handler):
        return self._inner.on_error(handler)

    # --------------------------------------------------------------
    # Serve / close
    # --------------------------------------------------------------

    def serve(self) -> None:
        """Block until :meth:`close` is called."""
        submit(self._inner.serve()).result()

    def close(self) -> None:
        """Stop accepting and close every active peer."""
        try:
            submit(self._inner.close()).result(timeout=5)
        except Exception:
            pass

    def __enter__(self) -> "SyncServer":
        return self

    def __exit__(self, *_: object) -> None:
        self.close()
