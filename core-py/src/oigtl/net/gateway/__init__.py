"""``oigtl.net.gateway`` — transport-neutral gateway pattern.

Ships the four-shape vocabulary
(:class:`Endpoint` / :class:`Acceptor` / :class:`Connector` /
:class:`Middleware`), the :func:`gateway` pipeline driver, and TCP
adapters. Future transports (WebSocket, MQTT, file replay) drop in
as additional adapter modules without changes to the driver.

The design contract: gateways run OIGTL bytes end-to-end. They
do not decode or translate. Middleware can filter or tag, but the
wire payload that enters one endpoint is the same payload that
leaves the other.

Minimum example — TCP → TCP bridge with a filter::

    from oigtl.net.gateway import gateway, TcpAcceptor, TcpConnector

    class DropImages:
        async def upstream_to_downstream(self, msg):
            return None if msg.header.type_id == "IMAGE" else msg
        async def downstream_to_upstream(self, msg):
            return msg

    await gateway(
        TcpAcceptor(18944),
        TcpConnector("tracker.lab", 18944),
        middleware=[DropImages()],
    )

Full design notes: ``core-py/NET_GUIDE.md`` (gateway section,
coming in Phase 8b).
"""

from __future__ import annotations

from oigtl.net._options import RawMessage
from oigtl.net.gateway.bridge import bridge, gateway
from oigtl.net.gateway.tcp import (
    TcpAcceptor,
    TcpClientEndpoint,
    TcpConnector,
    TcpPeerEndpoint,
)
from oigtl.net.gateway.ws import (
    WsAcceptor,
    WsClientEndpoint,
    WsConnector,
    WsPeerEndpoint,
)
from oigtl.net.gateway.types import (
    Acceptor,
    Connector,
    Endpoint,
    Middleware,
    MiddlewareHook,
)

__all__ = [
    # Abstractions
    "Acceptor",
    "Connector",
    "Endpoint",
    "Middleware",
    "MiddlewareHook",
    "RawMessage",
    # Driver
    "bridge",
    "gateway",
    # TCP adapters
    "TcpAcceptor",
    "TcpClientEndpoint",
    "TcpConnector",
    "TcpPeerEndpoint",
    # WS adapters
    "WsAcceptor",
    "WsClientEndpoint",
    "WsConnector",
    "WsPeerEndpoint",
]
