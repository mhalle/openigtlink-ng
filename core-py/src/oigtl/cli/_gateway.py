"""``oigtl gateway`` — transport bridges.

Currently one subcommand — ``oigtl gateway run --from URL --to URL``.
URL schemes dispatch via :mod:`oigtl.cli._urls` (``tcp://`` and
``ws://`` now; ``mqtt://`` and ``file://`` reserved for future
adapters — those error with a helpful message pointing at the
design sketch).

Examples::

    # Browser ↔ legacy TCP tracker
    oigtl gateway run --from ws://:18945/ --to tcp://tracker.lab:18944

    # Reverse: TCP publisher ↔ WS consumer
    oigtl gateway run --from tcp://:18944 --to ws://viewer.lab:18945/

The researcher sees a simple blocking command that reports each
new pair as peers arrive. Ctrl-C (SIGINT) or SIGTERM tears down
cleanly — both sides' ``close()`` methods fire in order.
"""

from __future__ import annotations

import asyncio
import signal

import typer

from oigtl.cli._urls import parse_acceptor, parse_connector
from oigtl.net.gateway import Acceptor, Connector, gateway

__all__ = ["gateway_app"]


gateway_app = typer.Typer(
    help="Transport gateway bridges (tcp://, ws://).",
    no_args_is_help=True,
)


@gateway_app.command("run")
def run(
    from_: str = typer.Option(
        ..., "--from", "-f",
        help="Acceptor URL: tcp://:PORT, ws://:PORT, "
             "tcp://HOST:PORT (bind to a specific interface).",
    ),
    to: str = typer.Option(
        ..., "--to", "-t",
        help="Connector URL: tcp://HOST:PORT, ws://HOST:PORT/.",
    ),
) -> None:
    """Bridge every accepted upstream peer to a freshly-dialled downstream.

    The gateway is a byte pipe — OIGTL wire bytes flow end-to-end
    without decoding. Runs until Ctrl-C / SIGTERM.
    """
    try:
        up = parse_acceptor(from_)
        down = parse_connector(to)
    except ValueError as e:
        typer.echo(f"error: {e}", err=True)
        raise typer.Exit(code=2)
    except NotImplementedError as e:
        typer.echo(f"error: {e}", err=True)
        raise typer.Exit(code=2)

    try:
        asyncio.run(_run_gateway(up, down))
    except KeyboardInterrupt:
        # Second-line defence; the signal handler usually catches
        # SIGINT first.
        pass


async def _run_gateway(up: Acceptor, down: Connector) -> None:
    typer.echo(
        f"oigtl gateway: {type(up).__name__} -> {type(down).__name__} "
        f"(Ctrl-C to stop)",
        err=True,
    )

    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, stop.set)
        except NotImplementedError:
            # Windows doesn't support add_signal_handler for SIGTERM.
            # Ctrl-C still works via KeyboardInterrupt at asyncio.run.
            pass

    gateway_task = asyncio.create_task(gateway(up, down))

    # Wait until either a signal flips `stop` or the gateway task
    # terminates on its own (Acceptor closed, for example).
    stop_task = asyncio.create_task(stop.wait())
    done, _pending = await asyncio.wait(
        {gateway_task, stop_task},
        return_when=asyncio.FIRST_COMPLETED,
    )

    gateway_task.cancel()
    stop_task.cancel()
    try:
        await gateway_task
    except (asyncio.CancelledError, Exception):
        pass

    try:
        await up.close()
    except Exception:
        pass
