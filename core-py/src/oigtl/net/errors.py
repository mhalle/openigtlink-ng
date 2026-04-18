"""Transport-layer exception hierarchy.

Mirrors ``oigtl::transport::*Error`` in
``core-cpp/include/oigtl/transport/errors.hpp``.

The codec layer's :mod:`oigtl.runtime.exceptions` hierarchy stays
separate — a malformed body surfaces as
:class:`~oigtl.runtime.exceptions.ProtocolError` through a
receive, while a peer-side FIN surfaces as
:class:`ConnectionClosedError`. Callers can distinguish.
"""

from __future__ import annotations


class TransportError(Exception):
    """Base for every transport-layer failure."""


class ConnectionClosedError(TransportError):
    """The peer closed the connection, or the local side called ``close()``.

    Any pending ``receive()`` resolves with this, and any further
    ``send()`` raises it (unless resilience features intercept —
    see :class:`BufferOverflowError`).
    """

    def __init__(self, message: str = "connection closed") -> None:
        super().__init__(message)


class OperationCancelledError(TransportError):
    """A pending operation was cancelled (``close()`` during receive)."""

    def __init__(self, message: str = "operation cancelled") -> None:
        super().__init__(message)


class TimeoutError(TransportError):  # noqa: A001 — mirrors C++ name
    """A ``receive()`` / ``send()`` exceeded its wall-clock budget.

    Named to match ``oigtl::transport::TimeoutError`` from the C++
    side. Shadows the builtin ``TimeoutError`` only inside this
    module; import as ``from oigtl.net.errors import TimeoutError``
    at call sites.
    """

    def __init__(self, message: str = "operation timed out") -> None:
        super().__init__(message)


class FramingError(TransportError):
    """Framer rejected the bytes on the wire.

    Wire-level protocol errors (bad CRC, malformed header) surface
    as :class:`~oigtl.runtime.exceptions.ProtocolError` subclasses;
    this one is reserved for framer-specific issues such as a
    body_size exceeding ``max_message_size``.
    """


class BufferOverflowError(ConnectionClosedError):
    """``send()`` while the offline buffer is full, ``DropNewest`` policy.

    Derives from :class:`ConnectionClosedError` so callers that
    already catch "can't send right now" keep working — a
    buffer-full event is morally equivalent to "the wire isn't
    accepting writes at the moment".
    """

    def __init__(self, message: str = "offline buffer full") -> None:
        super().__init__(message)
