"""TCP adapters for the gateway pattern.

Wrap the existing :class:`~oigtl.net.Client` and
:class:`~oigtl.net.Server` in the :class:`~oigtl.net.gateway.Endpoint`
/ :class:`~oigtl.net.gateway.Acceptor` / :class:`~oigtl.net.gateway.Connector`
shapes so the transport-neutral :func:`gateway` function can drive
them.

These adapters are deliberately thin — ~30 LoC each — because the
heavy lifting (framing, accept loop, restrictions) is already in
the Client/Server layer. Adding a new transport is then a
similarly-sized adapter against that transport's library.
"""

from __future__ import annotations

from typing import AsyncIterator

from oigtl.net._options import ClientOptions, RawMessage
from oigtl.net.client import Client
from oigtl.net.server import Peer, Server, ServerOptions

__all__ = [
    "TcpAcceptor",
    "TcpClientEndpoint",
    "TcpConnector",
    "TcpPeerEndpoint",
]


# ---------------------------------------------------------------------------
# Endpoints — one per Peer / Client.
# ---------------------------------------------------------------------------


class TcpPeerEndpoint:
    """:class:`Endpoint` around a server-side :class:`Peer`."""

    def __init__(self, peer: Peer) -> None:
        self._peer = peer

    @property
    def peer_name(self) -> str:
        return str(self._peer.address)

    async def send_raw(self, msg: RawMessage) -> None:
        await self._peer.send_raw(msg)

    def raw_messages(self) -> AsyncIterator[RawMessage]:
        return self._peer.raw_messages()

    async def close(self) -> None:
        await self._peer.close()


class TcpClientEndpoint:
    """:class:`Endpoint` around an outbound :class:`Client`."""

    def __init__(self, client: Client) -> None:
        self._client = client

    @property
    def peer_name(self) -> str:
        peer = self._client.peer
        return f"{peer[0]}:{peer[1]}" if peer else "tcp-client"

    async def send_raw(self, msg: RawMessage) -> None:
        await self._client.send_raw(msg)

    def raw_messages(self) -> AsyncIterator[RawMessage]:
        return self._client.raw_messages()

    async def close(self) -> None:
        await self._client.close()


# ---------------------------------------------------------------------------
# Acceptor / Connector for TCP.
# ---------------------------------------------------------------------------


class TcpAcceptor:
    """Accept TCP peers on *port*, yield them as :class:`Endpoint` instances.

    The underlying :class:`Server` is created lazily on first
    :meth:`accepted` call so construction stays sync and error-free.
    Restrictions set on the server (``allow``, ``max_clients``,
    ``idle_timeout``, ``max_message_size``) apply identically to the
    gateway path — rejected peers never reach the iterator.
    """

    def __init__(
        self,
        port: int,
        options: ServerOptions | None = None,
        *,
        host: str = "0.0.0.0",
    ) -> None:
        self._port = port
        self._options = options
        self._host = host
        self._server: Server | None = None

    async def accepted(self) -> AsyncIterator[TcpPeerEndpoint]:
        self._server = await Server.listen(
            self._port, self._options, host=self._host,
        )
        async for peer in self._server.accepted_peers():
            yield TcpPeerEndpoint(peer)

    async def close(self) -> None:
        if self._server is not None:
            await self._server.close()
            self._server = None

    @property
    def server(self) -> Server | None:
        """Access to the underlying :class:`Server`, once accepting.

        Exposed so callers can apply restriction builders before the
        first peer arrives::

            acceptor = TcpAcceptor(18944)
            task = asyncio.create_task(run_gateway(acceptor, ...))
            # Wait for the server to bind, then restrict.
            while acceptor.server is None:
                await asyncio.sleep(0.01)
            acceptor.server.restrict_to_local_subnet()

        For most use cases, prefer configuring :class:`ServerOptions`
        up front and passing it to the constructor.
        """
        return self._server


class TcpConnector:
    """Dial a TCP :class:`Client` to (*host*, *port*) on each call."""

    def __init__(
        self,
        host: str,
        port: int,
        options: ClientOptions | None = None,
    ) -> None:
        self._host = host
        self._port = port
        self._options = options

    async def connect(self) -> TcpClientEndpoint:
        client = await Client.connect(
            self._host, self._port, self._options,
        )
        return TcpClientEndpoint(client)
