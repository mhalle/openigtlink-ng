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

from oigtl.net import interfaces
from oigtl.net._options import ClientOptions, Envelope, as_timedelta
from oigtl.net.client import Client
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
    # Client
    "Client",
    "ClientOptions",
    "Envelope",
    "as_timedelta",
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
