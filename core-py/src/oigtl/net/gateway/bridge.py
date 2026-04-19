"""The transport-neutral gateway pipeline.

Takes an :class:`Acceptor` and a :class:`Connector`, pairs each
accepted upstream peer with a freshly-dialled downstream endpoint,
and pipes raw IGTL bytes between them in both directions.

The design contract:

- OIGTL bytes run end-to-end. The gateway does not decode or
  re-encode message bodies. Byte-identical payloads flow from the
  upstream endpoint to the downstream endpoint and back.
- Middleware hooks can filter (return None) or tag
  ``RawMessage.attributes``. Middleware that rewrites ``wire``
  bytes is allowed but must keep the CRC valid; the gateway
  doesn't re-compute it.
- Pairing is one-to-one: one upstream peer ↔ one downstream
  connection. Fan-in / fan-out patterns are compositions of
  multiple gateway instances or custom adapters.

Typical uses:

- **WS ↔ TCP bridge**: ``gateway(WsAcceptor(18945), TcpConnector("tracker.lab", 18944))``
- **TCP recording**: ``gateway(TcpAcceptor(18944), FileRecordingConnector("/tmp/log.igtl"))``
- **MQTT bridge** (future): ``gateway(MqttAcceptor(...), TcpConnector(...))``
"""

from __future__ import annotations

import asyncio
import contextlib
from typing import Awaitable, Callable, Sequence

from oigtl.net._options import RawMessage
from oigtl.net.gateway.types import Acceptor, Connector, Endpoint, Middleware

__all__ = ["gateway", "bridge"]


#: Callback fired each time an upstream/downstream pair is
#: established. Signature: ``(upstream, downstream) -> Awaitable``.
#: Receives both endpoints *before* the bidirectional pipe starts,
#: so callers can stash references for later close / metrics.
PairCallback = Callable[[Endpoint, Endpoint], Awaitable[None]]


async def gateway(
    upstream: Acceptor,
    downstream: Connector,
    *,
    middleware: Sequence[Middleware] = (),
    on_pair: PairCallback | None = None,
) -> None:
    """Run a gateway: each upstream peer → one fresh downstream link.

    Blocks until the acceptor closes or the caller cancels the
    returned coroutine. Each peer gets its own task running the
    bidirectional pipe; a broken pair closes independently of the
    others.

    Args:
        upstream: Source of incoming peers (the "listen side").
        downstream: Dials a fresh link per upstream peer.
        middleware: Optional per-direction transforms/filters.
        on_pair: Called once per successful (up, down) pair, before
            the pipe starts. Useful for wiring observers.
    """
    active: set[asyncio.Task] = set()
    try:
        async for up in upstream.accepted():
            task = asyncio.create_task(
                _handle_pair(up, downstream, middleware, on_pair),
                name=f"gateway-pair-{up.peer_name}",
            )
            active.add(task)
            task.add_done_callback(active.discard)
    finally:
        for t in list(active):
            t.cancel()
        if active:
            await asyncio.gather(*active, return_exceptions=True)


async def _handle_pair(
    up: Endpoint,
    downstream: Connector,
    middleware: Sequence[Middleware],
    on_pair: PairCallback | None,
) -> None:
    """Dial the downstream, invoke on_pair, run the bidirectional pipe."""
    try:
        down = await downstream.connect()
    except Exception:
        # Downstream unreachable — close the upstream peer so they
        # get a clean EOF rather than a silent accept.
        await up.close()
        return

    if on_pair is not None:
        try:
            await on_pair(up, down)
        except Exception:
            # on_pair failure shouldn't kill the pair — log via an
            # error-handler layer if the caller wants it surfaced.
            pass

    try:
        await bridge(up, down, middleware=middleware)
    finally:
        # Always close both sides; idempotent if either already
        # closed during the pipe.
        await asyncio.gather(
            _safe_close(up), _safe_close(down),
            return_exceptions=True,
        )


async def bridge(
    a: Endpoint,
    b: Endpoint,
    *,
    middleware: Sequence[Middleware] = (),
) -> None:
    """Pipe messages between *a* and *b* in both directions.

    Returns when either side closes. Cancels the surviving
    direction cleanly. Intended as the inner loop of
    :func:`gateway` but also usable standalone when you already
    have two connected endpoints and just want them wired together.
    """
    async def forward(
        src: Endpoint,
        dst: Endpoint,
        hook_name: str,
    ) -> None:
        try:
            async for msg in src.raw_messages():
                m: RawMessage | None = msg
                for mw in middleware:
                    hook = getattr(mw, hook_name)
                    m = await hook(m)
                    if m is None:
                        break
                if m is not None:
                    try:
                        await dst.send_raw(m)
                    except Exception:
                        # Destination died — stop forwarding.
                        return
        except Exception:
            # Source died — stop forwarding.
            return

    t1 = asyncio.create_task(
        forward(a, b, "upstream_to_downstream"),
        name="gateway-a-to-b",
    )
    t2 = asyncio.create_task(
        forward(b, a, "downstream_to_upstream"),
        name="gateway-b-to-a",
    )
    # Wait for EITHER direction to finish, then tear down the other.
    done, pending = await asyncio.wait(
        {t1, t2}, return_when=asyncio.FIRST_COMPLETED,
    )
    for t in pending:
        t.cancel()
    await asyncio.gather(*pending, return_exceptions=True)


async def _safe_close(endpoint: Endpoint) -> None:
    with contextlib.suppress(Exception):
        await endpoint.close()
