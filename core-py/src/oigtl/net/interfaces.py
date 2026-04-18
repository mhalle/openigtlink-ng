"""Network-interface introspection, research-lab-first.

Two levels of API. Reach for the first three 95 % of the time:

    >>> from oigtl.net import interfaces
    >>> interfaces.primary_address()          # "the IP to share with
    IPv4Address('192.168.1.42')               #  my colleague"
    >>> interfaces.subnets()                  # "the LANs I'm on"
    [IPv4Network('192.168.1.0/24')]
    >>> interfaces.addresses()                # "every IP I hold"
    [IPv4Address('192.168.1.42'), IPv6Address('2001:db8::42')]

Full detail when you actually need to enumerate hardware:

    >>> for iface in interfaces.enumerate():
    ...     print(iface.name, list(iface.addresses))

The return values are stdlib :mod:`ipaddress` types (``IPv4Address``,
``IPv4Interface``, ``IPv4Network``) so anything Python already knows
how to do with them still works. Nothing new to learn.

Defaults exclude loopback (``127.0.0.1``, ``::1``) and link-local
(``169.254.*``, ``fe80::*``) because those are almost never what a
researcher asking "what's my IP" means. Opt back in with the
``include_loopback=True`` / ``include_link_local=True`` kwargs.

Under the hood we use :mod:`ifaddr` (a tiny pure-Python dependency);
the module imports cleanly on every platform we support.
"""

from __future__ import annotations

import ipaddress
from dataclasses import dataclass
from typing import Literal, Union

import ifaddr

__all__ = [
    "IPAddress",
    "IPInterface",
    "IPNetwork",
    "NetworkInterface",
    "enumerate",
    "addresses",
    "subnets",
    "primary_address",
]


# Type aliases so downstream code doesn't have to repeat the union.
# Using the old-style Union so the names exist at runtime (useful
# for isinstance checks in user code).
IPAddress = Union[ipaddress.IPv4Address, ipaddress.IPv6Address]
IPNetwork = Union[ipaddress.IPv4Network, ipaddress.IPv6Network]
IPInterface = Union[ipaddress.IPv4Interface, ipaddress.IPv6Interface]


# ------------------------------------------------------------------
# The one data type we own: a host network interface.
# ------------------------------------------------------------------


@dataclass(frozen=True)
class NetworkInterface:
    """One network interface on this host.

    Attributes:
        name: OS-visible name — ``'en0'`` on macOS, ``'eth0'`` on Linux,
            ``'Wi-Fi'`` on Windows.
        addresses: Every IP assigned to the interface, as stdlib
            :class:`~ipaddress.IPv4Interface` / :class:`~ipaddress.IPv6Interface`
            objects (address + prefix + network, bundled). A dual-stack
            interface typically has two — one v4, one v6.

    Derived views are available as properties so there's no risk of a
    stale copy. All cheap.
    """

    name: str
    addresses: tuple[IPInterface, ...]

    @property
    def subnets(self) -> tuple[IPNetwork, ...]:
        """The subnet of each address on this interface."""
        return tuple(a.network for a in self.addresses)

    @property
    def is_loopback(self) -> bool:
        """True if this is the host's loopback interface.

        Defined as "holds at least one loopback address" rather than
        "holds only loopback addresses" — Darwin's ``lo0`` carries
        ``127.0.0.1``, ``::1``, *and* a ``fe80::1`` link-local alias,
        so the stricter rule would misclassify it.
        """
        return any(a.is_loopback for a in self.addresses)


# ------------------------------------------------------------------
# enumerate — the fundamental query, everything else derives from it.
# ------------------------------------------------------------------


def enumerate() -> list[NetworkInterface]:
    """Every network interface visible on this host.

    Returns a list (possibly empty in a locked-down container).
    Callers who only want the IPs or subnets should reach for
    :func:`addresses` or :func:`subnets` instead — both one-liners.
    """
    # Shadowing the builtin `enumerate` at module scope is intentional:
    # callers write `interfaces.enumerate()`, which is unambiguous.
    # Inside this function we don't need the builtin.
    out: list[NetworkInterface] = []
    for adapter in ifaddr.get_adapters():
        addrs: list[IPInterface] = []
        for ip in adapter.ips:
            # ifaddr surfaces v4 ips as strings and v6 ips as
            # (literal, flowinfo, scope) tuples. Strip any
            # ``%scope`` suffix so ``ipaddress`` accepts it.
            literal = ip.ip[0] if isinstance(ip.ip, tuple) else ip.ip
            literal = literal.split("%", 1)[0]
            try:
                addrs.append(
                    ipaddress.ip_interface(f"{literal}/{ip.network_prefix}")
                )
            except ValueError:
                # Malformed address — skip the entry rather than
                # blow up the whole enumeration.
                continue
        if addrs:
            out.append(NetworkInterface(
                name=adapter.nice_name or adapter.name,
                addresses=tuple(addrs),
            ))
    return out


# ------------------------------------------------------------------
# addresses / subnets — the two "flat view" one-liners.
# ------------------------------------------------------------------


def addresses(
    *,
    family: Literal[4, 6] | None = None,
    include_loopback: bool = False,
    include_link_local: bool = False,
) -> list[IPAddress]:
    """Every IP this host holds.

    Args:
        family: If ``4`` or ``6``, restrict to that address family.
            ``None`` (default) returns both.
        include_loopback: If False (default), drops 127.0.0.1 / ::1.
        include_link_local: If False (default), drops 169.254.*/fe80::*.

    Returns a flat list with interface-grouping flattened out. Order
    mirrors :func:`enumerate` — typically "physical first, virtual
    after", but that's an OS-level decision, not a guarantee.
    """
    out: list[IPAddress] = []
    for iface in enumerate():
        for addr in iface.addresses:
            ip = addr.ip
            if family is not None and ip.version != family:
                continue
            if ip.is_loopback and not include_loopback:
                continue
            if ip.is_link_local and not include_link_local:
                continue
            out.append(ip)
    return out


def subnets(
    *,
    family: Literal[4, 6] | None = None,
    include_loopback: bool = False,
    include_link_local: bool = False,
) -> list[IPNetwork]:
    """Every subnet this host is a member of.

    The destination for :meth:`~oigtl.net.Server.restrict_to_local_subnet`::

        server = Server.listen(18944)
        server.allow(interfaces.subnets())   # LAN-only in one line

    Duplicates (same subnet reachable via multiple interfaces) are
    collapsed; order follows first-seen.
    """
    seen: set[IPNetwork] = set()
    out: list[IPNetwork] = []
    for iface in enumerate():
        for addr in iface.addresses:
            net = addr.network
            if family is not None and net.version != family:
                continue
            if addr.ip.is_loopback and not include_loopback:
                continue
            if addr.ip.is_link_local and not include_link_local:
                continue
            if net not in seen:
                seen.add(net)
                out.append(net)
    return out


# ------------------------------------------------------------------
# primary_address — the star utility. Research-first ergonomics.
# ------------------------------------------------------------------


def primary_address(
    *,
    family: Literal[4, 6] | None = None,
) -> IPAddress | None:
    """The IP a researcher would tell a colleague to connect to.

    Selection rules, in order:

    1. Private addresses (RFC 1918 v4, ULA fc00::/7 v6) over public.
       Lab setups almost always live on a LAN, not a publicly
       routable address.
    2. IPv4 over IPv6, unless ``family=6`` is requested.
    3. Never loopback or link-local.

    Returns ``None`` if the host has no usable address — rare, but
    possible in locked-down containers. Callers that need a deterministic
    "fail the test" path can check for ``None`` and raise.

    Args:
        family: Force a specific family (``4`` or ``6``) when you know
            which you need. ``None`` picks per the rules above.

    Example::

        >>> interfaces.primary_address()
        IPv4Address('192.168.1.42')
        >>> interfaces.primary_address(family=6)
        IPv6Address('2001:db8::42')
    """
    candidates = addresses(family=family)
    if not candidates:
        return None

    def rank(ip: IPAddress) -> tuple[int, int]:
        # Lower rank = better. Private beats public; v4 beats v6
        # (unless the caller narrowed to v6, in which case every
        # candidate is v6 and this tiebreaker is moot).
        return (
            0 if ip.is_private else 1,
            0 if ip.version == 4 else 1,
        )

    return min(candidates, key=rank)
