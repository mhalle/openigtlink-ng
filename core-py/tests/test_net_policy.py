"""Unit tests for ``oigtl.net.policy``.

Mirrors the coverage of the C++ ``policy_test.cpp`` on parse
behaviour, containment, and :class:`PeerPolicy` defaults.
"""

from __future__ import annotations

import ipaddress
from datetime import timedelta

import pytest

from oigtl.net.policy import (
    IpRange,
    PeerPolicy,
    parse,
    parse_cidr,
    parse_ip,
    parse_range,
)


# ----------------------------- parse_ip --------------------------------


def test_parse_ip_v4_single() -> None:
    r = parse_ip("10.1.2.42")
    assert r is not None
    assert r.version == 4
    assert r.first == r.last
    assert r.contains("10.1.2.42")
    assert not r.contains("10.1.2.43")


def test_parse_ip_v6_single() -> None:
    r = parse_ip("fd00::1")
    assert r is not None
    assert r.version == 6
    assert r.contains("fd00::1")
    assert not r.contains("fd00::2")


def test_parse_ip_rejects_garbage() -> None:
    assert parse_ip("not an ip") is None
    assert parse_ip("10.1.2.999") is None
    assert parse_ip("") is None


def test_parse_ip_tolerates_whitespace() -> None:
    assert parse_ip("  10.0.0.1  ") is not None


# ----------------------------- parse_cidr ------------------------------


def test_parse_cidr_v4() -> None:
    r = parse_cidr("10.1.2.0/24")
    assert r is not None
    assert r.contains("10.1.2.0")
    assert r.contains("10.1.2.255")
    assert not r.contains("10.1.3.0")


def test_parse_cidr_v4_non_aligned_ok() -> None:
    # strict=False inside means "10.1.2.42/24" is treated as 10.1.2.0/24.
    r = parse_cidr("10.1.2.42/24")
    assert r is not None
    assert r.contains("10.1.2.0")


def test_parse_cidr_v6() -> None:
    r = parse_cidr("fd00::/8")
    assert r is not None
    assert r.contains("fd00::1")
    assert r.contains("fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
    assert not r.contains("fe00::1")


def test_parse_cidr_rejects_garbage() -> None:
    assert parse_cidr("10.1.2.0") is None            # no slash
    assert parse_cidr("10.1.2.0/33") is None         # out of range
    assert parse_cidr("10.1.2.0/-1") is None
    assert parse_cidr("fd00::/129") is None


# ----------------------------- parse_range -----------------------------


def test_parse_range_v4() -> None:
    r = parse_range("10.1.2.1", "10.1.2.10")
    assert r is not None
    assert r.contains("10.1.2.1")
    assert r.contains("10.1.2.10")
    assert r.contains("10.1.2.5")
    assert not r.contains("10.1.2.11")


def test_parse_range_rejects_mixed_family() -> None:
    assert parse_range("10.1.2.1", "fd00::1") is None


def test_parse_range_rejects_first_gt_last() -> None:
    assert parse_range("10.1.2.10", "10.1.2.1") is None


# ----------------------------- parse (combined) ------------------------


def test_parse_dispatch_single() -> None:
    r = parse("10.1.2.42")
    assert r is not None and r.first == r.last


def test_parse_dispatch_cidr() -> None:
    r = parse("10.1.2.0/24")
    assert r is not None and r.first != r.last


def test_parse_dispatch_range_dash() -> None:
    r = parse("10.1.2.1-10.1.2.10")
    assert r is not None and r.first != r.last
    assert r.contains("10.1.2.5")


def test_parse_dispatch_range_spaces() -> None:
    r = parse("10.1.2.1 - 10.1.2.10")
    assert r is not None
    assert r.contains("10.1.2.5")


def test_parse_rejects_empty() -> None:
    assert parse("") is None
    assert parse("   ") is None


# ----------------------------- IpRange ---------------------------------


def test_iprange_contains_cross_family_false() -> None:
    r = parse_cidr("10.1.2.0/24")
    assert r is not None
    assert not r.contains("fd00::1")


def test_iprange_rejects_mismatched_family() -> None:
    with pytest.raises(ValueError):
        IpRange(
            first=ipaddress.IPv4Address("10.0.0.1"),
            last=ipaddress.IPv6Address("fd00::1"),     # type: ignore[arg-type]
        )


def test_iprange_rejects_first_gt_last() -> None:
    with pytest.raises(ValueError):
        IpRange(
            first=ipaddress.IPv4Address("10.0.0.10"),
            last=ipaddress.IPv4Address("10.0.0.1"),
        )


def test_iprange_str_single_and_range() -> None:
    r_single = parse_ip("10.1.2.42")
    assert str(r_single) == "10.1.2.42"
    r_range = parse_range("10.1.2.1", "10.1.2.5")
    assert r_range is not None
    assert str(r_range) == "10.1.2.1 - 10.1.2.5"


# ----------------------------- PeerPolicy ------------------------------


def test_peer_policy_defaults_allow_any() -> None:
    p = PeerPolicy()
    assert p.allowed_peers == []
    assert p.max_concurrent_connections == 0
    assert p.idle_timeout == timedelta(0)
    assert p.max_message_size == 0
    # Empty allow-list means "any peer".
    assert p.is_peer_allowed("10.1.2.3")
    assert p.is_peer_allowed("fd00::1")


def test_peer_policy_allow_list_narrows() -> None:
    p = PeerPolicy(allowed_peers=[parse("10.1.2.0/24")])  # type: ignore[list-item]
    assert p.is_peer_allowed("10.1.2.5")
    assert not p.is_peer_allowed("10.1.3.1")
    assert not p.is_peer_allowed("fd00::1")


def test_peer_policy_multiple_ranges_union() -> None:
    p = PeerPolicy(allowed_peers=[
        parse("10.1.2.0/24"),           # type: ignore[list-item]
        parse("192.168.0.0/16"),        # type: ignore[list-item]
    ])
    assert p.is_peer_allowed("10.1.2.9")
    assert p.is_peer_allowed("192.168.5.5")
    assert not p.is_peer_allowed("172.16.0.1")


def test_peer_policy_rejects_garbage_string() -> None:
    p = PeerPolicy(allowed_peers=[parse("10.1.2.0/24")])  # type: ignore[list-item]
    assert not p.is_peer_allowed("not an ip")
