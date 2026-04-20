"""``oigtl interfaces`` — local network interface introspection.

Thin CLI wrapper over :mod:`oigtl.net.interfaces`. Three subcommands
cover the common shell-level questions:

- ``oigtl interfaces list`` — every interface with its addresses.
- ``oigtl interfaces primary`` — the one IP you'd paste in a colleague's
  config.
- ``oigtl interfaces subnets`` — the list of LANs this host is on
  (directly pipeable into ``Server.restrict_to_local_subnet`` from
  code that reads stdin).
"""

from __future__ import annotations

from typing import Literal, cast

import typer

from oigtl.net import interfaces as _ifaces

__all__ = ["interfaces_app"]


interfaces_app = typer.Typer(
    help="Local network interface info.",
    no_args_is_help=True,
)


@interfaces_app.command("list")
def list_interfaces() -> None:
    """Print every network interface and its addresses."""
    for iface in _ifaces.enumerate():
        flags: list[str] = []
        if iface.is_loopback:
            flags.append("loopback")
        flag_str = f"  [{', '.join(flags)}]" if flags else ""
        typer.echo(f"{iface.name}{flag_str}")
        for addr in iface.addresses:
            typer.echo(f"  {addr}")


@interfaces_app.command("primary")
def primary(
    family: int | None = typer.Option(
        None, "--family", "-f",
        help="Restrict to a specific IP family (4 or 6).",
    ),
) -> None:
    """Print the primary (RFC-1918 / ULA preferred) address.

    Exits with status 1 if no non-loopback address is available
    (rare — e.g. in a locked-down container).
    """
    fam = _family_literal(family)
    addr = _ifaces.primary_address(family=fam)
    if addr is None:
        typer.echo("no non-loopback address available", err=True)
        raise typer.Exit(code=1)
    typer.echo(str(addr))


@interfaces_app.command("subnets")
def subnets(
    family: int | None = typer.Option(
        None, "--family", "-f",
        help="Restrict to a specific IP family (4 or 6).",
    ),
    include_loopback: bool = typer.Option(
        False, "--include-loopback",
        help="Include loopback subnets (127.0.0.0/8, ::1/128).",
    ),
    include_link_local: bool = typer.Option(
        False, "--include-link-local",
        help="Include link-local subnets (169.254.0.0/16, fe80::/10).",
    ),
) -> None:
    """Print every subnet this host is a member of, one per line."""
    fam = _family_literal(family)
    for net in _ifaces.subnets(
        family=fam,
        include_loopback=include_loopback,
        include_link_local=include_link_local,
    ):
        typer.echo(str(net))


def _family_literal(
    family: int | None,
) -> Literal[4, 6] | None:
    if family is None:
        return None
    if family not in (4, 6):
        raise typer.BadParameter(
            f"--family must be 4 or 6, got {family}"
        )
    return cast("Literal[4, 6]", family)
