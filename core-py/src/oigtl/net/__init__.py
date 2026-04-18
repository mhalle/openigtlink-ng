"""``oigtl.net`` — transport layer for the OpenIGTLink Python port.

Phase 1 only exports the shared primitives (framer, policy,
interfaces). Client / Server entry points land in later phases.
See ``core-py/TRANSPORT_PLAN.md`` for the full roadmap and
``core-cpp/CLIENT_GUIDE.md`` for the C++ API this port mirrors.
"""

from __future__ import annotations

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
from oigtl.net.interfaces import (
    InterfaceAddress,
    InterfaceEnumerationUnavailable,
    enumerate_interfaces,
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
    # Interfaces
    "InterfaceAddress",
    "InterfaceEnumerationUnavailable",
    "enumerate_interfaces",
    # Policy
    "IpRange",
    "PeerPolicy",
    "parse",
    "parse_cidr",
    "parse_ip",
    "parse_range",
]
