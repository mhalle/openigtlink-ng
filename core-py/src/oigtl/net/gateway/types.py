"""Protocol-independent types for the gateway pattern.

A gateway is a byte pipe between two OIGTL endpoints. The payload
stays OIGTL-framed end-to-end; the gateway doesn't decode or
re-encode unless a :class:`Middleware` chooses to.

Four abstractions cover every gateway we ship or plan to ship:

- :class:`Endpoint` — one side of a link. Can send raw wire bytes
  and iterate incoming raw messages.
- :class:`Acceptor` — produces Endpoints as peers arrive (server
  role). TCP server, WS server, MQTT subscriber all fit.
- :class:`Connector` — dials an Endpoint on demand (client role).
  One pairing per incoming upstream peer.
- :class:`Middleware` — optional filter/transform on each direction.

Adapters wrap the existing :class:`~oigtl.net.Client` /
:class:`~oigtl.net.Server` and protocol-specific libraries behind
these four shapes. The :func:`~oigtl.net.gateway.gateway` function
doesn't care which transport is on either side.
"""

from __future__ import annotations

from typing import (
    AsyncIterator,
    Awaitable,
    Callable,
    Protocol,
    runtime_checkable,
)

from oigtl.net._options import RawMessage

__all__ = [
    "Acceptor",
    "Connector",
    "Endpoint",
    "Middleware",
    "MiddlewareHook",
    "RawMessage",
]


@runtime_checkable
class Endpoint(Protocol):
    """One side of a gateway link — transport-agnostic.

    Implementations are thin wrappers around the existing
    :class:`Client` / :class:`Peer` (for TCP) or protocol-specific
    libraries (WS, MQTT, etc.). The three methods below are the
    entire contract; everything above :class:`Endpoint` stays
    protocol-neutral.

    Attributes:
        peer_name: Human-readable identity for logs
            (e.g., ``"127.0.0.1:51234"``, ``"ws://client-17"``).
    """

    peer_name: str

    async def send_raw(self, msg: RawMessage) -> None:
        """Forward *msg* on this endpoint. Raises on transport failure."""
        ...

    def raw_messages(self) -> AsyncIterator[RawMessage]:
        """Async iterator over raw messages received on this endpoint.

        Ends on peer close or local :meth:`close`.
        """
        ...

    async def close(self) -> None:
        """Close the underlying transport. Idempotent."""
        ...


@runtime_checkable
class Acceptor(Protocol):
    """Produces :class:`Endpoint` instances as peers arrive.

    The server role of a gateway's "listen side". Canonical
    examples: TCP server bound to a port, WebSocket server,
    MQTT broker-subscriber listening on a topic.
    """

    def accepted(self) -> AsyncIterator[Endpoint]: ...

    async def close(self) -> None: ...


@runtime_checkable
class Connector(Protocol):
    """Dials one :class:`Endpoint` on demand.

    Called once per incoming upstream peer — each upstream gets
    its own freshly-dialled downstream, preserving one-to-one
    pairing. Canonical examples: TCP client, WebSocket client,
    MQTT publisher.
    """

    async def connect(self) -> Endpoint: ...


# ---------------------------------------------------------------------------
# Middleware
# ---------------------------------------------------------------------------


#: A middleware hook: returns the message (possibly modified) to
#: forward, or ``None`` to drop it. Runs per-direction so filters
#: can be asymmetric (e.g., drop IMAGE going upstream but allow
#: STATUS going downstream).
MiddlewareHook = Callable[[RawMessage], Awaitable[RawMessage | None]]


@runtime_checkable
class Middleware(Protocol):
    """Transform or filter in-flight messages on a gateway link.

    Implementations can log, rate-limit, filter by type_id, rewrite
    payloads (at their own risk — the CRC must stay valid), or tag
    ``RawMessage.attributes`` with per-hop metadata.

    Middleware receives the message *before* the message is handed
    to the other endpoint. Returning ``None`` drops the message;
    returning the same or a modified :class:`RawMessage` forwards it.
    """

    async def upstream_to_downstream(
        self, msg: RawMessage,
    ) -> RawMessage | None: ...

    async def downstream_to_upstream(
        self, msg: RawMessage,
    ) -> RawMessage | None: ...
