"""Tests for the ``oigtl`` CLI.

Uses Click's :class:`CliRunner` (re-exported through Typer) so each
test invokes the real Typer app with no subprocesses. The actual
gateway ``run`` loop is covered by the existing net-layer tests;
here we verify the CLI surface: argument parsing, URL dispatch,
exit codes, and output lines a researcher relies on.
"""

from __future__ import annotations

import ipaddress

import pytest
from typer.testing import CliRunner

from oigtl.cli import app
from oigtl.cli._urls import parse_acceptor, parse_connector
from oigtl.net.gateway import (
    TcpAcceptor,
    TcpConnector,
    WsAcceptor,
    WsConnector,
)


runner = CliRunner()


# ---------------------------------------------------------------------------
# `oigtl info`
# ---------------------------------------------------------------------------


def test_info_prints_version_python_and_transports():
    result = runner.invoke(app, ["info"])
    assert result.exit_code == 0, result.output
    assert "oigtl" in result.output
    assert "python" in result.output
    # tcp is always available; ws should also be (websockets is a
    # base dep), so both should print.
    assert "tcp" in result.output
    assert "ws" in result.output
    assert "message types" in result.output


# ---------------------------------------------------------------------------
# `oigtl interfaces ...`
# ---------------------------------------------------------------------------


def test_interfaces_list_succeeds():
    result = runner.invoke(app, ["interfaces", "list"])
    assert result.exit_code == 0
    # lo / lo0 — name varies by platform — but we should at least
    # see some output on any reasonable test host.
    assert result.output.strip() != ""


def test_interfaces_primary_prints_ip_or_exits_1():
    result = runner.invoke(app, ["interfaces", "primary"])
    if result.exit_code == 0:
        # Parses as an IP address.
        ipaddress.ip_address(result.output.strip())
    else:
        # Locked-down container with no non-loopback addresses.
        assert result.exit_code == 1


def test_interfaces_primary_family_4():
    result = runner.invoke(app, ["interfaces", "primary", "-f", "4"])
    if result.exit_code == 0:
        addr = ipaddress.ip_address(result.output.strip())
        assert addr.version == 4


def test_interfaces_primary_rejects_bad_family():
    result = runner.invoke(app, ["interfaces", "primary", "-f", "5"])
    assert result.exit_code != 0


def test_interfaces_subnets_includes_loopback_opt_in():
    # Default output may be empty in a container; with loopback
    # included, we should always see at least 127.0.0.0/8 or ::1/128.
    result = runner.invoke(
        app, ["interfaces", "subnets", "--include-loopback"],
    )
    assert result.exit_code == 0
    lines = result.output.strip().splitlines()
    found_loopback = any(
        _net_contains_loopback(line) for line in lines
    )
    assert found_loopback, (
        f"expected a loopback subnet; got:\n{result.output}"
    )


def _net_contains_loopback(line: str) -> bool:
    try:
        net = ipaddress.ip_network(line.strip(), strict=False)
    except ValueError:
        return False
    probe_v4 = ipaddress.IPv4Address("127.0.0.1")
    probe_v6 = ipaddress.IPv6Address("::1")
    if net.version == 4:
        return probe_v4 in net
    return probe_v6 in net


# ---------------------------------------------------------------------------
# `oigtl gateway run` — argument parsing / URL dispatch
# ---------------------------------------------------------------------------


def test_gateway_run_rejects_unknown_scheme():
    result = runner.invoke(
        app,
        ["gateway", "run",
         "--from", "frob://whatever",
         "--to",   "tcp://127.0.0.1:18944"],
    )
    assert result.exit_code == 2
    assert "unsupported URL scheme" in (result.output + str(result.stderr_bytes or ""))


def test_gateway_run_rejects_mqtt_with_helpful_message():
    result = runner.invoke(
        app,
        ["gateway", "run",
         "--from", "mqtt://broker.lab/openigtlink/#",
         "--to",   "tcp://127.0.0.1:18944"],
    )
    assert result.exit_code == 2
    # Expect the "not yet implemented" + NET_GUIDE pointer text.
    combined = result.output + (result.stderr or "")
    assert "not yet implemented" in combined or "NET_GUIDE" in combined


def test_gateway_run_rejects_missing_port():
    result = runner.invoke(
        app,
        ["gateway", "run",
         "--from", "tcp://",
         "--to",   "tcp://host:18944"],
    )
    assert result.exit_code == 2


# ---------------------------------------------------------------------------
# URL parser — direct unit tests (don't touch the CLI at all)
# ---------------------------------------------------------------------------


def test_parse_acceptor_tcp_port_only():
    a = parse_acceptor("tcp://:18944")
    assert isinstance(a, TcpAcceptor)


def test_parse_acceptor_tcp_host_and_port():
    a = parse_acceptor("tcp://0.0.0.0:18944")
    assert isinstance(a, TcpAcceptor)


def test_parse_acceptor_ws():
    a = parse_acceptor("ws://:18945")
    assert isinstance(a, WsAcceptor)


def test_parse_acceptor_mqtt_deferred():
    with pytest.raises(NotImplementedError) as exc:
        parse_acceptor("mqtt://broker/topic")
    assert "mqtt" in str(exc.value).lower()


def test_parse_acceptor_file_deferred():
    with pytest.raises(NotImplementedError):
        parse_acceptor("file:///tmp/corpus.bin")


def test_parse_acceptor_wss_deferred():
    with pytest.raises(NotImplementedError):
        parse_acceptor("wss://lab:18945")


def test_parse_acceptor_requires_port():
    with pytest.raises(ValueError):
        parse_acceptor("tcp://")


def test_parse_acceptor_rejects_unknown_scheme():
    with pytest.raises(ValueError):
        parse_acceptor("gopher://example/")


def test_parse_connector_tcp():
    c = parse_connector("tcp://127.0.0.1:18944")
    assert isinstance(c, TcpConnector)


def test_parse_connector_ws_full_url():
    c = parse_connector("ws://tracker.lab:18945/")
    assert isinstance(c, WsConnector)


def test_parse_connector_tcp_needs_host():
    with pytest.raises(ValueError):
        parse_connector("tcp://:18944")


def test_parse_connector_mqtt_deferred():
    with pytest.raises(NotImplementedError):
        parse_connector("mqtt://broker/topic")


def test_parse_connector_rejects_unknown_scheme():
    with pytest.raises(ValueError):
        parse_connector("carrier-pigeon://tracker.lab")


# ---------------------------------------------------------------------------
# Top-level dispatch shape
# ---------------------------------------------------------------------------


def test_bare_invocation_shows_help():
    """Running `oigtl` with no arguments should show help and exit 2."""
    result = runner.invoke(app, [])
    # Typer's no_args_is_help=True makes this exit with 2.
    assert result.exit_code == 2
    for name in ("info", "interfaces", "gateway"):
        assert name in result.output


def test_help_includes_all_top_level_commands():
    result = runner.invoke(app, ["--help"])
    assert result.exit_code == 0
    for name in ("info", "interfaces", "gateway"):
        assert name in result.output


def test_gateway_help_mentions_supported_schemes():
    result = runner.invoke(app, ["gateway", "run", "--help"])
    assert result.exit_code == 0
    assert "tcp://" in result.output
    assert "ws://" in result.output
