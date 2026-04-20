"""``oigtl`` CLI — top-level command dispatch.

Shipped as a console_scripts entry point in ``pyproject.toml``:

    [project.scripts]
    oigtl = "oigtl.cli:main"

After ``uv sync`` / ``pip install``, users get an ``oigtl``
command on $PATH. Also reachable as ``python -m oigtl``.

Structure: one Typer app per logical group, assembled here.
Keeps each subcommand module tightly focused — if ``oigtl
gateway`` grows more subcommands, they land in ``_gateway.py``
without touching the others.

First-pass subcommands:

- ``oigtl info`` — version + capabilities.
- ``oigtl interfaces {list,primary,subnets}`` — local network info.
- ``oigtl gateway run --from URL --to URL`` — transport bridges.

Deferred (anticipated structure, not yet implemented):

- ``oigtl send`` / ``oigtl listen`` — one-shot send/receive.
- ``oigtl serve`` — echo/log server for debugging.
- ``oigtl record`` / ``oigtl replay`` — capture / playback files.
- ``oigtl messages {list,show}`` — schema introspection.
- MQTT adapters surface through the existing gateway URL
  dispatch (``mqtt://...``) — no new subcommand group needed.
"""

from __future__ import annotations

import typer

from oigtl.cli._gateway import gateway_app
from oigtl.cli._info import info
from oigtl.cli._interfaces import interfaces_app

__all__ = ["app", "main"]


app = typer.Typer(
    name="oigtl",
    help="OpenIGTLink toolkit — transport, introspection, and bridges.",
    no_args_is_help=True,
    add_completion=False,
    # ``rich`` formatting is nice for humans but clutters CI logs
    # and breaks diffs in golden-output tests. Plain text keeps the
    # output predictable.
    rich_markup_mode=None,
)

# Commands & groups. Order here is the order they appear in --help.
app.command(
    name="info",
    help="Print oigtl version, Python version, and available transports.",
)(info)
app.add_typer(interfaces_app, name="interfaces")
app.add_typer(gateway_app, name="gateway")


def main() -> None:
    """Entry point for ``oigtl`` / ``python -m oigtl``."""
    app()
