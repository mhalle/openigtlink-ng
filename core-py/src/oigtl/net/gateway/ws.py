"""WebSocket adapters for the gateway pattern.

Same shape as the TCP adapters in :mod:`.tcp`. The
:func:`~oigtl.net.gateway.gateway` driver is transport-neutral, so
once these exist the researcher's WS↔TCP bridge is a single call::

    from oigtl.net.gateway import gateway, WsAcceptor, TcpConnector

    await gateway(
        WsAcceptor(18945),                          # browser side
        TcpConnector("tracker.lab", 18944),         # native TCP tracker
    )

No WSS yet.
"""

from __future__ import annotations

from typing import AsyncIterator

# Side-effect import: attaches Server.listen_ws().
import oigtl.net.ws_server  # noqa: F401
from oigtl.net._options import ClientOptions, RawMessage
from oigtl.net.server import Peer, Server, ServerOptions
from oigtl.net.ws_client import WsClient

__all__ = [
    "WsAcceptor",
    "WsClientEndpoint",
    "WsConnector",
    "WsPeerEndpoint",
]


# ----------------------------------------------------------------------
# Endpoints
# ----------------------------------------------------------------------


class WsPeerEndpoint:
    """:class:`Endpoint` around a server-side WebSocket :class:`Peer`."""

    def __init__(self, peer: Peer) -> None:
        self._peer = peer

    @property
    def peer_name(self) -> str:
        return f"ws://{self._peer.address}"

    async def send_raw(self, msg: RawMessage) -> None:
        await self._peer.send_raw(msg)

    def raw_messages(self) -> AsyncIterator[RawMessage]:
        return self._peer.raw_messages()

    async def close(self) -> None:
        await self._peer.close()


class WsClientEndpoint:
    """:class:`Endpoint` around an outbound :class:`WsClient`."""

    def __init__(self, client: WsClient) -> None:
        self._client = client

    @property
    def peer_name(self) -> str:
        return self._client.url

    async def send_raw(self, msg: RawMessage) -> None:
        await self._client.send_raw(msg)

    def raw_messages(self) -> AsyncIterator[RawMessage]:
        return self._client.raw_messages()

    async def close(self) -> None:
        await self._client.close()


# ----------------------------------------------------------------------
# Acceptor / Connector
# ----------------------------------------------------------------------


class WsAcceptor:
    """Accept WebSocket peers on *port*; yield them as endpoints.

    The underlying :class:`Server` is created lazily on first
    :meth:`accepted` call (via :meth:`Server.listen_ws`).
    Restrictions set via :attr:`server` propagate identically to
    the TCP acceptor.
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

    async def accepted(self) -> AsyncIterator[WsPeerEndpoint]:
        self._server = await Server.listen_ws(   # type: ignore[attr-defined]
            self._port, self._options, host=self._host,
        )
        async for peer in self._server.accepted_peers():
            yield WsPeerEndpoint(peer)

    async def close(self) -> None:
        if self._server is not None:
            await self._server.close()
            self._server = None

    @property
    def server(self) -> Server | None:
        return self._server


class WsConnector:
    """Dial a WebSocket peer at *url* on each call."""

    def __init__(
        self,
        url: str,
        options: ClientOptions | None = None,
    ) -> None:
        if not url.startswith("ws://") and not url.startswith("wss://"):
            raise ValueError(
                f"WsConnector URL must start with ws:// or wss://; got {url!r}"
            )
        self._url = url
        self._options = options

    async def connect(self) -> WsClientEndpoint:
        client = await WsClient.connect(self._url, self._options)
        return WsClientEndpoint(client)
