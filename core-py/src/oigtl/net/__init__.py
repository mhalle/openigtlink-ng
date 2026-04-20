"""``oigtl.net`` — transport layer for the OpenIGTLink Python port.

Phase 1 only exports the shared primitives (framer, policy,
interfaces). Client / Server entry points land in later phases.
See ``core-py/TRANSPORT_PLAN.md`` for the full roadmap and
``core-cpp/CLIENT_GUIDE.md`` for the C++ API this port mirrors in
capability (not in syntax — Python gets a Python-shaped API).

The ``interfaces`` submodule is exposed as a namespace rather than
flattened here, because the researcher-facing idiom is::

    from oigtl.net import interfaces
    interfaces.primary_address()
    interfaces.subnets()

— which reads naturally and doesn't collide with builtins.
"""

from __future__ import annotations

from oigtl.net import gateway, interfaces
from oigtl.net._options import (
    ClientOptions,
    Envelope,
    OfflineOverflow,
    RawMessage,
    as_timedelta,
)
from oigtl.net.client import Client
from oigtl.net.server import Peer, PeerAddress, Server, ServerOptions, TcpPeer
# Side-effect import: attaches Server.listen_ws(). Kept here so a
# plain ``from oigtl.net import Server`` always exposes the WS
# classmethod; users don't need a separate import line.
import oigtl.net.ws_server as _ws_server_module  # noqa: F401
from oigtl.net.ws_client import WsClient
from oigtl.net.ws_server import WsPeer
from oigtl.net.sync_client import SyncClient
from oigtl.net.sync_server import SyncServer
from oigtl.net.errors import (
    BufferOverflowError,
    ConnectionClosedError,
    FramingError,
    OperationCancelledError,
    TimeoutError,
    TransportError,
)
from oigtl.net.framer import (
    HEADER_SIZE,
    Framer,
    FramerMetadata,
    Incoming,
    V3Framer,
    make_v3_framer,
)
from oigtl.net.policy import (
    IpRange,
    PeerPolicy,
    parse,
    parse_cidr,
    parse_ip,
    parse_range,
)

__all__ = [
    # Submodule namespaces
    "interfaces",
    "gateway",
    # Client
    "Client",
    "WsClient",
    "SyncClient",
    "ClientOptions",
    "OfflineOverflow",
    "Envelope",
    "RawMessage",
    "as_timedelta",
    # Server
    "Server",
    "SyncServer",
    "ServerOptions",
    "Peer",
    "PeerAddress",
    "TcpPeer",
    "WsPeer",
    # Errors
    "BufferOverflowError",
    "ConnectionClosedError",
    "FramingError",
    "OperationCancelledError",
    "TimeoutError",
    "TransportError",
    # Framer
    "HEADER_SIZE",
    "Framer",
    "FramerMetadata",
    "Incoming",
    "V3Framer",
    "make_v3_framer",
    # Policy
    "IpRange",
    "PeerPolicy",
    "parse",
    "parse_cidr",
    "parse_ip",
    "parse_range",
]
