"""Accept-time policy for inbound connections.

Mirrors ``oigtl::transport::PeerPolicy`` / ``IpRange`` in
``core-cpp/include/oigtl/transport/policy.hpp``.

Every restriction is optional and composes additively. A default
:class:`PeerPolicy` accepts any peer — matching the pre-policy
behaviour and the C++ side.

Checks run at accept time, before the first IGTL byte is read. A
blocked peer is rejected by closing the stream writer before any
handler sees the connection, so the restrictions also serve as a
pre-parse DoS mitigation layer.

Python's :mod:`ipaddress` module does the byte-level parsing for
us, which keeps this considerably smaller than the C++
equivalent. We still expose :class:`IpRange` as the canonical
unit so code sharing with the server API is symmetric.
"""

from __future__ import annotations

import ipaddress
from dataclasses import dataclass, field
from datetime import timedelta
from typing import Union

__all__ = [
    "IpRange",
    "PeerPolicy",
    "parse_ip",
    "parse_cidr",
    "parse_range",
    "parse",
]


_IpAddress = Union[ipaddress.IPv4Address, ipaddress.IPv6Address]


@dataclass(frozen=True)
class IpRange:
    """One IPv4 or IPv6 address range, both endpoints inclusive.

    Parse via :func:`parse_ip`, :func:`parse_cidr`, :func:`parse_range`,
    or the combined :func:`parse`. Callers rarely construct this
    directly; if they do, they must pass two addresses of the same
    family with ``first <= last``.
    """

    first: _IpAddress
    last: _IpAddress

    def __post_init__(self) -> None:
        if self.first.version != self.last.version:
            raise ValueError(
                f"IpRange endpoints must share address family "
                f"(first=v{self.first.version}, last=v{self.last.version})"
            )
        if int(self.first) > int(self.last):
            raise ValueError(
                f"IpRange first ({self.first}) must be <= last ({self.last})"
            )

    @property
    def version(self) -> int:
        """4 or 6."""
        return self.first.version

    def contains(self, peer: _IpAddress | str) -> bool:
        """Does this range contain ``peer``?

        Accepts a parsed address or a string. Different-family peers
        always answer ``False``.
        """
        if isinstance(peer, str):
            try:
                peer = ipaddress.ip_address(peer)
            except ValueError:
                return False
        if peer.version != self.version:
            return False
        return int(self.first) <= int(peer) <= int(self.last)

    def __str__(self) -> str:
        if self.first == self.last:
            return str(self.first)
        return f"{self.first} - {self.last}"


# ------------------------------------------------------------------
# Parsers — one spec form each, plus a combined :func:`parse`.
# ------------------------------------------------------------------


def parse_ip(spec: str) -> IpRange | None:
    """Parse a single IPv4 or IPv6 literal (e.g. ``"10.1.2.42"``, ``"::1"``).

    Returns ``None`` on malformed input. Hostnames are NOT resolved
    here — the caller is responsible for DNS lookup if it wants to
    accept a name.
    """
    try:
        addr = ipaddress.ip_address(spec.strip())
    except ValueError:
        return None
    return IpRange(first=addr, last=addr)


def parse_cidr(spec: str) -> IpRange | None:
    """Parse CIDR notation (e.g. ``"10.1.2.0/24"``, ``"fd00::/8"``).

    Returns ``None`` on malformed input.
    """
    spec = spec.strip()
    if "/" not in spec:
        return None
    try:
        # strict=False lets "10.1.2.42/24" normalize to "10.1.2.0/24".
        net = ipaddress.ip_network(spec, strict=False)
    except ValueError:
        return None
    return IpRange(first=net.network_address, last=net.broadcast_address)


def parse_range(first_spec: str, last_spec: str) -> IpRange | None:
    """Parse a two-endpoint range (inclusive).

    Both endpoints must be the same family and ``first <= last``.
    Returns ``None`` on malformed input.
    """
    try:
        first = ipaddress.ip_address(first_spec.strip())
        last = ipaddress.ip_address(last_spec.strip())
    except ValueError:
        return None
    if first.version != last.version:
        return None
    if int(first) > int(last):
        return None
    return IpRange(first=first, last=last)


def parse(spec: str) -> IpRange | None:
    """Parse any of the supported forms.

    Accepts:

    - ``"10.1.2.42"`` — single host
    - ``"10.1.2.0/24"`` — CIDR
    - ``"10.1.2.1-10.1.2.254"`` — dash-separated range
    - ``"10.1.2.1 - 10.1.2.254"`` — spaces tolerated
    - ``"::1"`` — IPv6 single host
    - ``"fd00::/8"`` — IPv6 CIDR

    Returns ``None`` on malformed input. Hostnames are NOT resolved.
    """
    spec = spec.strip()
    if not spec:
        return None

    if "/" in spec:
        return parse_cidr(spec)

    # Dash-separated range? Careful with IPv6 — "::1" has no dashes,
    # but "fd00::1-fd00::ffff" has exactly one dash between two IPv6
    # addresses. The first dash in a non-CIDR spec is always the
    # separator (IPv6 addresses themselves don't contain dashes).
    dash = spec.find("-")
    if dash != -1:
        return parse_range(spec[:dash], spec[dash + 1:])

    return parse_ip(spec)


# ------------------------------------------------------------------
# PeerPolicy
# ------------------------------------------------------------------


@dataclass
class PeerPolicy:
    """The full accept-time policy.

    All fields default to "no restriction". Mutate and pass to the
    server API; the server checks each field at accept time.

    Attributes:
        allowed_peers: If empty, any peer is allowed. If non-empty,
            a peer is accepted iff its address lies in at least one
            range.
        max_concurrent_connections: 0 = unlimited. Connections
            already accepted but not yet closed count against this
            cap. A new connection arriving over the cap is rejected
            (accepted briefly then closed).
        idle_timeout: A :class:`~datetime.timedelta` of 0 disables
            the timeout. Otherwise, a connection with no received
            bytes for this duration is closed.
        max_message_size: 0 = no per-message cap (body_size is still
            bounded by the 64-bit field). Non-zero: a wire message
            with ``body_size`` greater than this triggers a
            :class:`~oigtl.net.errors.FramingError` and connection
            close before body bytes are buffered.
    """

    allowed_peers: list[IpRange] = field(default_factory=list)
    max_concurrent_connections: int = 0
    idle_timeout: timedelta = timedelta(0)
    max_message_size: int = 0

    def is_peer_allowed(self, peer: _IpAddress | str) -> bool:
        """Does the current ``allowed_peers`` list admit this peer?

        An empty list means "no peer filter" — always returns
        ``True``. Same semantics as the C++ side.
        """
        if not self.allowed_peers:
            return True
        if isinstance(peer, str):
            try:
                peer = ipaddress.ip_address(peer)
            except ValueError:
                return False
        return any(r.contains(peer) for r in self.allowed_peers)
