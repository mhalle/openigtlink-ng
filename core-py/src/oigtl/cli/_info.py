"""``oigtl info`` — runtime version + capability probe.

Prints a few lines every CLI user expects to be able to reach:

- the installed ``oigtl`` version
- the Python interpreter
- the number of typed message classes in the REGISTRY
- the set of transports currently importable (tcp is always there;
  ws if ``websockets`` is installed; mqtt once ``aiomqtt`` lands)

Useful for bug reports and for "did my install actually pick up
the optional transports?" questions.
"""

from __future__ import annotations

import sys
from importlib.metadata import PackageNotFoundError, version as _pkg_version

import typer

__all__ = ["info"]


def info() -> None:
    """Print version and runtime capabilities."""
    typer.echo(f"oigtl         {_oigtl_version()}")
    typer.echo(f"python        {sys.version.split()[0]}")
    typer.echo(f"message types {_message_type_count()}")
    typer.echo(f"transports    {', '.join(_available_transports())}")


def _oigtl_version() -> str:
    try:
        return _pkg_version("oigtl")
    except PackageNotFoundError:
        return "unknown (not installed via pip/uv)"


def _message_type_count() -> int:
    # Local import so `oigtl info` doesn't pay the REGISTRY-population
    # cost on every invocation if Typer runs a quick subcommand.
    # (It's cheap; keeping the import lazy just keeps the CLI honest.)
    from oigtl.messages import REGISTRY
    return len(REGISTRY)


def _available_transports() -> list[str]:
    out = ["tcp"]
    try:
        import websockets  # noqa: F401
        out.append("ws")
    except ImportError:
        pass
    try:
        import aiomqtt  # noqa: F401
        out.append("mqtt")
    except ImportError:
        pass
    return out
