"""Unit tests for ``oigtl.net.interfaces``.

The real backends depend on whether ``psutil`` / ``ifaddr`` are
installed in the test environment; we smoke-test the public surface
rather than mock OS calls. The contract we care about:

1. ``enumerate_interfaces()`` returns a list (possibly empty).
2. Every entry has a well-formed :class:`IpRange` subnet.
3. At least one loopback entry is present when a backend is
   available (every reasonable machine has 127.0.0.1 or ::1).
"""

from __future__ import annotations

import ipaddress

import pytest

from oigtl.net.interfaces import InterfaceAddress, enumerate_interfaces
from oigtl.net.policy import IpRange


def _has_backend() -> bool:
    try:
        import psutil  # noqa: F401 — probe only
        return True
    except ImportError:
        pass
    try:
        import ifaddr  # noqa: F401 — probe only
        return True
    except ImportError:
        return False


def test_enumerate_returns_list() -> None:
    result = enumerate_interfaces()
    assert isinstance(result, list)
    for item in result:
        assert isinstance(item, InterfaceAddress)
        assert isinstance(item.subnet, IpRange)


@pytest.mark.skipif(not _has_backend(),
                    reason="needs psutil or ifaddr installed")
def test_enumerate_finds_loopback() -> None:
    result = enumerate_interfaces()
    assert result, "backend available but no interfaces returned"
    loopbacks = [a for a in result if a.is_loopback]
    assert loopbacks, "expected at least one loopback interface"
    # The loopback subnet should contain 127.0.0.1 or ::1.
    found_ipv4 = any(a.subnet.contains("127.0.0.1") for a in loopbacks)
    found_ipv6 = any(a.subnet.contains("::1") for a in loopbacks)
    assert found_ipv4 or found_ipv6


@pytest.mark.skipif(not _has_backend(),
                    reason="needs psutil or ifaddr installed")
def test_every_subnet_is_valid_range() -> None:
    for addr in enumerate_interfaces():
        # endpoints must be same-family (enforced by IpRange ctor).
        assert addr.subnet.first.version == addr.subnet.last.version
        assert int(addr.subnet.first) <= int(addr.subnet.last)
        # The address family matches the flag interpretations.
        if addr.is_link_local:
            # IPv4 APIPA or IPv6 fe80::/10.
            if addr.subnet.first.version == 4:
                apipa = ipaddress.IPv4Network("169.254.0.0/16")
                assert ipaddress.IPv4Address(
                    int(addr.subnet.first)) in apipa or \
                    ipaddress.IPv4Address(
                    int(addr.subnet.last)) in apipa
            else:
                linklocal = ipaddress.IPv6Network("fe80::/10")
                assert ipaddress.IPv6Address(
                    int(addr.subnet.first)) in linklocal or \
                    ipaddress.IPv6Address(
                    int(addr.subnet.last)) in linklocal
