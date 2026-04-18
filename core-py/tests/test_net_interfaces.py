"""Tests for the researcher-facing ``oigtl.net.interfaces`` API.

The module has five public entry points — we cover each:

- ``enumerate()`` — full detail, must find at least one interface.
- ``addresses()`` — flat IP list, filters applied correctly.
- ``subnets()`` — deduped subnet list.
- ``primary_address()`` — picks something sensible (or None).
- ``NetworkInterface`` — derived views are self-consistent.

We don't mock the OS; we exercise the public contract on whatever
interfaces pytest is running on. ``ifaddr`` is a hard dep so every
path has a backend.
"""

from __future__ import annotations

import ipaddress
from typing import cast

from oigtl.net import interfaces
from oigtl.net.interfaces import NetworkInterface


# ---------- enumerate() ----------


def test_enumerate_returns_at_least_loopback() -> None:
    ifaces = interfaces.enumerate()
    assert ifaces, "every host has at least a loopback interface"
    assert all(isinstance(i, NetworkInterface) for i in ifaces)


def test_every_interface_has_a_name_and_addresses() -> None:
    for iface in interfaces.enumerate():
        assert iface.name                   # non-empty
        assert iface.addresses              # at least one address
        for addr in iface.addresses:
            assert isinstance(addr, (
                ipaddress.IPv4Interface,
                ipaddress.IPv6Interface,
            ))


def test_network_interface_derived_views_are_consistent() -> None:
    for iface in interfaces.enumerate():
        # subnets property matches manual extraction.
        assert iface.subnets == tuple(a.network for a in iface.addresses)
        # is_loopback == "has at least one loopback address".
        expected_loop = any(a.is_loopback for a in iface.addresses)
        assert iface.is_loopback == expected_loop


def test_loopback_interface_present_somewhere() -> None:
    ifaces = interfaces.enumerate()
    loop_ifaces = [i for i in ifaces if i.is_loopback]
    assert loop_ifaces, "expected at least one loopback interface"
    # Loopback interface contains 127.0.0.1 or ::1.
    all_ips = [a.ip for i in loop_ifaces for a in i.addresses]
    assert any(ip == ipaddress.IPv4Address("127.0.0.1") for ip in all_ips) \
        or any(ip == ipaddress.IPv6Address("::1") for ip in all_ips)


# ---------- addresses() ----------


def test_addresses_excludes_loopback_by_default() -> None:
    result = interfaces.addresses()
    assert all(not ip.is_loopback for ip in result)


def test_addresses_includes_loopback_when_asked() -> None:
    result = interfaces.addresses(include_loopback=True)
    has_loopback = any(ip.is_loopback for ip in result)
    assert has_loopback, "include_loopback=True should surface 127.0.0.1/::1"


def test_addresses_excludes_link_local_by_default() -> None:
    result = interfaces.addresses()
    assert all(not ip.is_link_local for ip in result)


def test_addresses_family_filter() -> None:
    v4 = interfaces.addresses(family=4)
    v6 = interfaces.addresses(family=6)
    assert all(ip.version == 4 for ip in v4)
    assert all(ip.version == 6 for ip in v6)
    # v4 ∪ v6 == unfiltered (as multisets).
    both = interfaces.addresses()
    assert sorted(map(str, both)) == sorted(map(str, v4 + v6))


# ---------- subnets() ----------


def test_subnets_excludes_loopback_by_default() -> None:
    # Every subnet should have at least one non-loopback address on
    # our host (by construction of the filter).
    result = interfaces.subnets()
    loop = {
        ipaddress.IPv4Network("127.0.0.0/8"),
        ipaddress.IPv6Network("::1/128"),
    }
    assert not (set(result) & loop)


def test_subnets_includes_loopback_when_asked() -> None:
    with_loop = interfaces.subnets(include_loopback=True)
    without = interfaces.subnets()
    assert len(with_loop) >= len(without)


def test_subnets_deduplicates() -> None:
    # Two interfaces with the same subnet would otherwise appear
    # twice. On a typical dev machine subnets are already unique,
    # but the invariant should hold either way.
    result = interfaces.subnets(include_loopback=True)
    assert len(result) == len(set(result))


def test_subnets_returns_ipnetwork_types() -> None:
    for net in interfaces.subnets(include_loopback=True):
        assert isinstance(net, (
            ipaddress.IPv4Network,
            ipaddress.IPv6Network,
        ))


# ---------- primary_address() ----------


def test_primary_address_on_a_normal_host() -> None:
    # Most dev boxes have at least one non-loopback address. If this
    # test runs on a nothing-but-loopback container it'd return None
    # legitimately; skip logic handled by the assertion below.
    all_ips = interfaces.addresses(include_loopback=True)
    non_loop = [ip for ip in all_ips if not ip.is_loopback
                and not ip.is_link_local]
    primary = interfaces.primary_address()
    if non_loop:
        assert primary is not None
        assert not primary.is_loopback
        assert not primary.is_link_local
    else:
        assert primary is None


def test_primary_address_prefers_private_over_public() -> None:
    # We can't synthesise adapters; instead sanity-check the ranking
    # function implicitly. If the host has any private address,
    # primary_address must be private (not a public one that happens
    # to appear first).
    pri = interfaces.primary_address()
    if pri is None:
        return
    has_private = any(
        ip.is_private and not ip.is_loopback and not ip.is_link_local
        for ip in interfaces.addresses(include_loopback=True)
    )
    if has_private:
        assert pri.is_private, (
            f"host has a private address but primary_address() "
            f"returned non-private {pri}"
        )


def test_primary_address_family_filter() -> None:
    v4 = interfaces.primary_address(family=4)
    v6 = interfaces.primary_address(family=6)
    if v4 is not None:
        assert v4.version == 4
    if v6 is not None:
        assert v6.version == 6


def test_primary_address_never_loopback_or_link_local() -> None:
    for family in (None, 4, 6):
        pri = interfaces.primary_address(
            family=cast("Literal[4, 6] | None", family),
        )
        if pri is not None:
            assert not pri.is_loopback
            assert not pri.is_link_local
