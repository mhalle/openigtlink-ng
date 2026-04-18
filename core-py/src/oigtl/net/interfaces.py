"""Enumerate the host's network interfaces.

Mirrors ``oigtl::transport::enumerate_interfaces`` in
``core-cpp/src/transport/policy.cpp`` (plus its platform backends
in ``net_compat_posix.cpp`` / ``net_compat_winsock.cpp``).

Python has no stdlib equivalent of ``getifaddrs`` /
``GetAdaptersAddresses`` that yields subnet masks — we need them
to implement :meth:`~oigtl.net.Server.restrict_to_local_subnet`.
We depend on ``ifaddr`` (tiny pure-Python, no C build step) so
the transport API surface works out of the box. If ``psutil`` is
also installed — common in research environments — we prefer it.
Both backends return the same :class:`InterfaceAddress` shape.
"""

from __future__ import annotations

import ipaddress
from dataclasses import dataclass

from oigtl.net.policy import IpRange

__all__ = [
    "InterfaceAddress",
    "enumerate_interfaces",
    "InterfaceEnumerationUnavailable",
]


@dataclass(frozen=True)
class InterfaceAddress:
    """One interface / address-family pair.

    Dual-stack interfaces produce two entries (one V4, one V6).
    """

    name: str           # "eth0", "en0", "Ethernet 2"
    subnet: IpRange     # the address + mask rendered as a range
    is_loopback: bool = False
    is_link_local: bool = False


class InterfaceEnumerationUnavailable(RuntimeError):
    """Interface enumeration failed on this host.

    ``ifaddr`` is a hard dependency, so this should not fire in a
    normal install. It's raised by callers that require a non-empty
    interface list (e.g. ``Server.restrict_to_local_subnet()``)
    when the OS refuses to enumerate — rare, but we keep the
    failure typed so misconfigured environments get a clean error
    rather than a silent allow-all.
    """


# ------------------------------------------------------------------
# Backends
# ------------------------------------------------------------------


def _classify(addr: ipaddress.IPv4Address | ipaddress.IPv6Address,
              ) -> tuple[bool, bool]:
    """Return (is_loopback, is_link_local) for an address.

    APIPA (169.254.0.0/16) and IPv6 ``fe80::/10`` are flagged
    link-local. Loopback uses the stdlib classifier.
    """
    return (addr.is_loopback, addr.is_link_local)


def _from_psutil() -> list[InterfaceAddress] | None:
    try:
        import psutil  # type: ignore[import-not-found]
    except ImportError:
        return None

    out: list[InterfaceAddress] = []
    for iface, addrs in psutil.net_if_addrs().items():
        for snic in addrs:
            # snic.family is socket.AF_INET / AF_INET6; we match on
            # the address text instead to avoid importing socket
            # constants here.
            try:
                addr = ipaddress.ip_address(snic.address.split("%", 1)[0])
            except (ValueError, AttributeError):
                continue
            netmask = snic.netmask
            if netmask is None:
                # Windows reports netmask=None for some interfaces;
                # fall back to /32 or /128.
                prefix = 32 if addr.version == 4 else 128
            elif addr.version == 4:
                try:
                    prefix = ipaddress.IPv4Network(
                        f"0.0.0.0/{netmask}"
                    ).prefixlen
                except ValueError:
                    continue
            else:
                # IPv6 netmask is either a prefix int as string or an
                # expanded-mask address. psutil on Linux gives prefix
                # strings; on Windows it gives expanded form.
                try:
                    prefix = int(netmask)
                except ValueError:
                    try:
                        prefix = ipaddress.IPv6Network(
                            f"::/{netmask}"
                        ).prefixlen
                    except ValueError:
                        continue
            try:
                net = ipaddress.ip_network(f"{addr}/{prefix}", strict=False)
            except ValueError:
                continue
            is_loop, is_link = _classify(addr)
            out.append(InterfaceAddress(
                name=iface,
                subnet=IpRange(
                    first=net.network_address,
                    last=net.broadcast_address,
                ),
                is_loopback=is_loop,
                is_link_local=is_link,
            ))
    return out


def _from_ifaddr() -> list[InterfaceAddress] | None:
    try:
        import ifaddr  # type: ignore[import-not-found]
    except ImportError:
        return None

    out: list[InterfaceAddress] = []
    for adapter in ifaddr.get_adapters():
        for ip in adapter.ips:
            ip_str = ip.ip[0] if isinstance(ip.ip, tuple) else ip.ip
            try:
                addr = ipaddress.ip_address(ip_str.split("%", 1)[0])
            except ValueError:
                continue
            try:
                net = ipaddress.ip_network(
                    f"{addr}/{ip.network_prefix}", strict=False,
                )
            except ValueError:
                continue
            is_loop, is_link = _classify(addr)
            out.append(InterfaceAddress(
                name=adapter.nice_name or adapter.name,
                subnet=IpRange(
                    first=net.network_address,
                    last=net.broadcast_address,
                ),
                is_loopback=is_loop,
                is_link_local=is_link,
            ))
    return out


def enumerate_interfaces() -> list[InterfaceAddress]:
    """Snapshot the local host's network interfaces.

    Filter rules (matching the C++ side):

    - Loopback included, flagged ``is_loopback=True``.
    - IPv4 APIPA (169.254.0.0/16) flagged ``is_link_local=True``.
    - IPv6 link-local (``fe80::/10``) flagged ``is_link_local=True``.
    - Interfaces that are DOWN are skipped by the underlying libs.

    Returns ``[]`` only in the rare case where the OS refuses to
    enumerate (both backends failed). Callers that must have the
    list should catch the empty return and raise
    :class:`InterfaceEnumerationUnavailable`.
    """
    for backend in (_from_psutil, _from_ifaddr):
        result = backend()
        if result is not None:
            return result
    return []
