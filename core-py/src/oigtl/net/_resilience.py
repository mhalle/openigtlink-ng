"""Resilience plumbing for :class:`~oigtl.net.Client` — shared with
the sync wrapper by composition.

Three moving parts:

1. **Keepalive** — :func:`configure_keepalive` applies SO_KEEPALIVE
   and the per-platform TCP_KEEPIDLE/TCP_KEEPINTVL/TCP_KEEPCNT knobs
   to an already-connected socket. Ported from the C++
   ``detail::net_compat::configure_keepalive``.

2. **Offline buffer** — :class:`OfflineBuffer` holds packed wire bytes
   (header + body already composed) while the connection is down,
   drains in FIFO order on reconnect, honours the three overflow
   policies. Packed bytes rather than typed messages so the drain
   path doesn't have to re-query timestamps or device names.

3. **Backoff** — :func:`compute_backoff` produces the next delay in
   the exponential-with-jitter schedule. Extracted as a pure
   function so tests can check the schedule deterministically.

Each piece is independently testable; the Client composes them but
doesn't own the logic.
"""

from __future__ import annotations

import asyncio
import random
import socket
from collections import deque
from dataclasses import dataclass
from datetime import timedelta

from oigtl.net._options import ClientOptions, OfflineOverflow
from oigtl.net.errors import (
    BufferOverflowError,
    ConnectionClosedError,
    TimeoutError as NetTimeoutError,
)

__all__ = [
    "compute_backoff",
    "configure_keepalive",
    "OfflineBuffer",
]


# ----------------------------------------------------------------------
# Keepalive
# ----------------------------------------------------------------------


def configure_keepalive(
    sock: socket.socket,
    *,
    idle: timedelta,
    interval: timedelta,
    count: int,
) -> None:
    """Enable SO_KEEPALIVE with tuned intervals on *sock*.

    Silently no-ops on knobs the platform doesn't expose — a partial
    configuration is still better than none. Callers that need a
    specific minimum aggressiveness should read the effective values
    back themselves; the OpenIGTLink use case doesn't need that.

    Per-platform coverage:

    - Linux: ``TCP_KEEPIDLE``, ``TCP_KEEPINTVL``, ``TCP_KEEPCNT`` —
      full.
    - macOS: ``TCP_KEEPALIVE`` (idle only) + ``TCP_KEEPINTVL``,
      ``TCP_KEEPCNT``. Idle goes through the macOS-specific constant.
    - Windows: SIO_KEEPALIVE_VALS via socket.ioctl. Takes idle and
      interval only (no count knob at the BSD-compat level).
    - Other platforms: SO_KEEPALIVE only.
    """
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)

    idle_s = max(1, int(idle.total_seconds()))
    interval_s = max(1, int(interval.total_seconds()))
    count = max(1, int(count))

    # Windows uses the SIO_KEEPALIVE_VALS ioctl with ms values.
    if hasattr(socket, "SIO_KEEPALIVE_VALS"):
        try:
            sock.ioctl(
                socket.SIO_KEEPALIVE_VALS,
                (1, idle_s * 1000, interval_s * 1000),
            )
        except OSError:
            pass
        return

    # Linux has TCP_KEEPIDLE; macOS spells it TCP_KEEPALIVE.
    tcp_keepidle = getattr(socket, "TCP_KEEPIDLE", None) \
        or getattr(socket, "TCP_KEEPALIVE", None)
    if tcp_keepidle is not None:
        try:
            sock.setsockopt(socket.IPPROTO_TCP, tcp_keepidle, idle_s)
        except OSError:
            pass

    if hasattr(socket, "TCP_KEEPINTVL"):
        try:
            sock.setsockopt(
                socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, interval_s,
            )
        except OSError:
            pass

    if hasattr(socket, "TCP_KEEPCNT"):
        try:
            sock.setsockopt(
                socket.IPPROTO_TCP, socket.TCP_KEEPCNT, count,
            )
        except OSError:
            pass


# ----------------------------------------------------------------------
# Backoff schedule
# ----------------------------------------------------------------------


def compute_backoff(
    attempt: int,
    opt: ClientOptions,
    *,
    rng: random.Random | None = None,
) -> timedelta:
    """Delay to wait before *attempt*, with jitter.

    *attempt* is 1-based: 1 is the first retry, 2 the second, etc.
    Backoff doubles each step capped at
    :attr:`ClientOptions.reconnect_max_backoff`; jitter multiplies
    by ``(1 ± jitter)`` drawn uniformly.
    """
    if attempt < 1:
        return timedelta(0)

    base = opt.reconnect_initial_backoff.total_seconds()
    cap = opt.reconnect_max_backoff.total_seconds()
    # 2**(attempt-1) grows without bound; the min cap prevents overflow.
    delay = min(cap, base * (2 ** (attempt - 1)))

    if opt.reconnect_backoff_jitter > 0:
        r = rng or random
        factor = 1 + r.uniform(
            -opt.reconnect_backoff_jitter,
            opt.reconnect_backoff_jitter,
        )
        delay *= factor

    return timedelta(seconds=max(0.0, delay))


# ----------------------------------------------------------------------
# Offline buffer
# ----------------------------------------------------------------------


@dataclass
class _PackedMessage:
    """One outgoing message as already-framed wire bytes.

    Stored as bytes (not a typed message) so the drain path doesn't
    need to re-pack, and the header's timestamp reflects when the
    caller submitted the send, not when the reconnect happened.
    """

    wire: bytes


class OfflineBuffer:
    """Bounded FIFO of outgoing wire bytes with three overflow policies.

    The buffer is filled by ``send()`` calls made while the
    connection is down, drained in order on reconnect before any
    new live sends. Thread-friendly: all mutations go through the
    event loop's lock (via an ``asyncio.Condition``).

    Capacity 0 means "no buffering" — ``push`` raises immediately.
    This is the default and matches the pre-Phase-4 behaviour.
    """

    def __init__(self, opt: ClientOptions) -> None:
        self._opt = opt
        self._cap = opt.offline_buffer_capacity
        self._queue: deque[_PackedMessage] = deque()
        # Condition so BLOCK-policy senders can wait for space.
        self._cv = asyncio.Condition()

    def __len__(self) -> int:
        return len(self._queue)

    @property
    def capacity(self) -> int:
        return self._cap

    async def push(self, wire: bytes) -> None:
        """Enqueue *wire*, honouring the overflow policy.

        Raises:
            BufferOverflowError: ``DROP_NEWEST`` policy and buffer full.
            TimeoutError: ``BLOCK`` policy and ``send_timeout`` elapsed.
            ConnectionClosedError: capacity is 0 (buffering disabled).
        """
        if self._cap == 0:
            raise ConnectionClosedError(
                "offline_buffer_capacity=0; send() while disconnected"
            )

        async with self._cv:
            if len(self._queue) < self._cap:
                self._queue.append(_PackedMessage(wire=wire))
                return

            # Full — apply policy.
            policy = self._opt.offline_overflow_policy
            if policy == OfflineOverflow.DROP_OLDEST:
                self._queue.popleft()
                self._queue.append(_PackedMessage(wire=wire))
                return

            if policy == OfflineOverflow.DROP_NEWEST:
                raise BufferOverflowError(
                    f"offline buffer full "
                    f"(capacity={self._cap}, policy=DROP_NEWEST)"
                )

            # BLOCK — wait for space up to send_timeout.
            assert policy == OfflineOverflow.BLOCK
            timeout = self._opt.send_timeout
            if timeout is None:
                await self._cv.wait_for(
                    lambda: len(self._queue) < self._cap
                )
            else:
                try:
                    await asyncio.wait_for(
                        self._cv.wait_for(
                            lambda: len(self._queue) < self._cap
                        ),
                        timeout=timeout.total_seconds(),
                    )
                except asyncio.TimeoutError as e:
                    raise NetTimeoutError(
                        f"send blocked on full offline buffer for "
                        f"{timeout}"
                    ) from e
            self._queue.append(_PackedMessage(wire=wire))

    async def drain(self, send_one) -> None:
        """Hand every buffered message to *send_one* in order.

        *send_one* is an async callable taking ``bytes`` and raising
        on send error. If it raises, the item that caused the failure
        stays at the head of the queue — the caller can reconnect
        and retry.
        """
        while True:
            async with self._cv:
                if not self._queue:
                    return
                item = self._queue[0]
            try:
                await send_one(item.wire)
            except Exception:
                # Leave the item at the head; caller decides whether
                # to retry after reconnect.
                raise
            async with self._cv:
                # We already have the head-of-queue invariant since
                # drain is the only consumer; still popleft once.
                if self._queue and self._queue[0] is item:
                    self._queue.popleft()
                self._cv.notify_all()

    async def notify_space(self) -> None:
        """Wake BLOCK-policy senders after a drain step."""
        async with self._cv:
            self._cv.notify_all()

    def clear(self) -> None:
        """Drop every buffered entry. Used during terminal shutdown."""
        self._queue.clear()
