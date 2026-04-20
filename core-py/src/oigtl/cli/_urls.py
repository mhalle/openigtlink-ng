"""URL-scheme dispatch for gateway endpoints.

Translates researcher-friendly URLs into the typed :class:`Acceptor`
/ :class:`Connector` implementations in :mod:`oigtl.net.gateway`.

Supported schemes:

===============  ======================================  ==================
Role in URL      Example                                 Target
===============  ======================================  ==================
Acceptor / from  ``tcp://:18944``                        :class:`TcpAcceptor`
Acceptor / from  ``tcp://0.0.0.0:18944``                 :class:`TcpAcceptor`
Acceptor / from  ``ws://:18945``                         :class:`WsAcceptor`
Connector / to   ``tcp://tracker.lab:18944``             :class:`TcpConnector`
Connector / to   ``ws://tracker.lab:18945/``             :class:`WsConnector`
Either           ``mqtt://broker.lab/openigtlink/#``     raises (future)
Either           ``file:///tmp/recording.igtl``          raises (future)
===============  ======================================  ==================

The ``mqtt://`` and ``file://`` schemes raise
:class:`NotImplementedError` with a pointer to the guide section
that sketches the future adapter. Adding an adapter is one extra
``elif`` in each dispatcher.
"""

from __future__ import annotations

from urllib.parse import urlparse

from oigtl.net.gateway import (
    Acceptor,
    Connector,
    TcpAcceptor,
    TcpConnector,
    WsAcceptor,
    WsConnector,
)

__all__ = ["parse_acceptor", "parse_connector"]


# ---------------------------------------------------------------------------
# Acceptor side (the --from URL in `oigtl gateway run`)
# ---------------------------------------------------------------------------


def parse_acceptor(url: str) -> Acceptor:
    """Construct an :class:`Acceptor` from *url*.

    Raises :class:`ValueError` on an unknown scheme or malformed
    host/port. Raises :class:`NotImplementedError` for schemes that
    are reserved but not yet built (``mqtt://``, ``file://``).
    """
    parsed = urlparse(url)
    scheme = parsed.scheme
    host = parsed.hostname or "0.0.0.0"

    if scheme == "tcp":
        port = _require_port(parsed.port, url)
        return TcpAcceptor(port, host=host)

    if scheme == "ws":
        port = _require_port(parsed.port, url)
        return WsAcceptor(port, host=host)

    if scheme == "wss":
        raise NotImplementedError(
            "wss:// (TLS) acceptors are not supported yet; "
            "use ws:// for now. See NET_GUIDE.md for the TLS plan."
        )

    if scheme == "mqtt":
        raise NotImplementedError(
            "mqtt:// acceptors are not yet implemented. "
            "See NET_GUIDE.md ('MQTT gateway') for the sketched design."
        )

    if scheme == "file":
        raise NotImplementedError(
            "file:// replay acceptors are not yet implemented."
        )

    raise ValueError(
        f"unsupported URL scheme {scheme!r} in --from {url!r}. "
        f"Known: tcp://, ws://."
    )


# ---------------------------------------------------------------------------
# Connector side (the --to URL in `oigtl gateway run`)
# ---------------------------------------------------------------------------


def parse_connector(url: str) -> Connector:
    """Construct a :class:`Connector` from *url*."""
    parsed = urlparse(url)
    scheme = parsed.scheme

    if scheme == "tcp":
        host = parsed.hostname
        if host is None:
            raise ValueError(
                f"tcp:// --to URL needs a host: {url!r}"
            )
        port = _require_port(parsed.port, url)
        return TcpConnector(host, port)

    if scheme == "ws":
        # WsConnector takes the full URL so path / query ride along.
        return WsConnector(url)

    if scheme == "wss":
        raise NotImplementedError(
            "wss:// (TLS) connectors are not supported yet; "
            "use ws:// for now."
        )

    if scheme == "mqtt":
        raise NotImplementedError(
            "mqtt:// connectors are not yet implemented. "
            "See NET_GUIDE.md for the sketched MqttConnector design."
        )

    if scheme == "file":
        raise NotImplementedError(
            "file:// record connectors are not yet implemented."
        )

    raise ValueError(
        f"unsupported URL scheme {scheme!r} in --to {url!r}. "
        f"Known: tcp://, ws://."
    )


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _require_port(port: int | None, url: str) -> int:
    if port is None:
        raise ValueError(f"URL needs a port: {url!r}")
    return port
