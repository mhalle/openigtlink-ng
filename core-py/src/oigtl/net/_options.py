"""Typed options for ``oigtl.net.Client`` / ``Server``.

Kept in a private module so the public API surface (imports from
``oigtl.net.client``) stays compact. Duration fields accept either
:class:`datetime.timedelta` or a bare ``int`` / ``float`` taken as
milliseconds — researchers writing quick scripts tend to reach for
``timeout=500`` rather than ``timeout=timedelta(milliseconds=500)``,
and both should work.
"""

from __future__ import annotations

from datetime import timedelta
from enum import Enum
from typing import Annotated, Generic, TypeVar

from pydantic import BaseModel, BeforeValidator, ConfigDict, Field

from oigtl.runtime.header import Header

__all__ = [
    "ClientOptions",
    "Envelope",
    "OfflineOverflow",
    "RawMessage",
    "as_timedelta",
]


class OfflineOverflow(str, Enum):
    """Policy for ``Client.send()`` when the offline buffer is full.

    Mirrors ``ClientOptions::OfflineOverflow`` in the C++ header.

    - ``DROP_OLDEST``: succeed by discarding the queued head — right
      for telemetry (pose, sensor readings) where old data is stale.
    - ``DROP_NEWEST`` (default): raise
      :class:`~oigtl.net.errors.BufferOverflowError` — right for
      commands / transactions where the problem should surface.
    - ``BLOCK``: wait up to ``send_timeout`` for space — right for
      strictly-ordered flows with bounded throughput.
    """

    DROP_OLDEST = "drop_oldest"
    DROP_NEWEST = "drop_newest"
    BLOCK = "block"


def _coerce_to_timedelta(value: object) -> timedelta | None:
    """Pydantic ``BeforeValidator`` for duration fields.

    Accepts:

    - :class:`~datetime.timedelta` (passed through).
    - ``int`` / ``float`` — interpreted as milliseconds. Matches the
      "researcher writes ``timeout=500``" idiom.
    - ``None`` — passed through; the field's type must include it.
    """
    if value is None or isinstance(value, timedelta):
        return value  # type: ignore[return-value]
    if isinstance(value, (int, float)):
        return timedelta(milliseconds=value)
    raise TypeError(
        f"duration fields accept timedelta or int/float ms, "
        f"got {type(value).__name__}"
    )


# Alias so downstream modules reading the code see the intent.
Duration = Annotated[timedelta, BeforeValidator(_coerce_to_timedelta)]
OptionalDuration = Annotated[
    timedelta | None, BeforeValidator(_coerce_to_timedelta),
]


def as_timedelta(
    value: timedelta | int | float | None,
) -> timedelta | None:
    """Coerce a duration argument the same way :class:`ClientOptions` does.

    Exposed because :meth:`Client.receive` and friends accept an
    inline ``timeout=`` argument that should honour the same rule as
    the option-struct fields.
    """
    return _coerce_to_timedelta(value)


class ClientOptions(BaseModel):
    """Configuration for :class:`~oigtl.net.client.Client`.

    Mirrors the capability of ``oigtl::ClientOptions`` in
    ``core-cpp/include/oigtl/client.hpp``. Phase 2 only consumes the
    connection/send/receive fields; resilience fields land in
    Phase 4.

    Duration fields (``connect_timeout``, ``receive_timeout``) accept
    either a :class:`~datetime.timedelta` or a bare number of
    milliseconds — ``ClientOptions(receive_timeout=500)`` is
    equivalent to ``ClientOptions(receive_timeout=timedelta(milliseconds=500))``.
    """

    model_config = ConfigDict(arbitrary_types_allowed=True)

    # ---- Connection identity -------------------------------------
    default_device: str = Field(
        default="python",
        description=(
            "Device name written into the header when send() isn't "
            "given an explicit device_name. OpenIGTLink receivers "
            "often route on this."
        ),
    )

    # ---- Timeouts -------------------------------------------------
    connect_timeout: OptionalDuration = timedelta(seconds=10)
    """Time budget for the initial TCP connect. ``None`` = no limit."""

    receive_timeout: OptionalDuration = None
    """Default wall-clock budget for ``receive()`` / ``receive_any()``
    when the caller doesn't pass a per-call timeout. ``None`` =
    block forever."""

    send_timeout: OptionalDuration = None
    """Budget for a blocked ``send()`` when the offline buffer is full
    and :attr:`offline_overflow_policy` is ``BLOCK``. ``None`` = wait
    forever. Has no effect on other policies."""

    # ---- Framer policy -------------------------------------------
    max_message_size: int = Field(
        default=0,
        description=(
            "If non-zero, an inbound message with body_size above "
            "this cap is rejected before the body bytes are read. "
            "Pre-parse DoS defence. 0 = no app-level cap (the "
            "64-bit wire field itself still bounds body_size)."
        ),
        ge=0,
    )

    # ---- Resilience: auto-reconnect ------------------------------
    auto_reconnect: bool = Field(
        default=False,
        description=(
            "When True, a connection drop after the initial connect "
            "succeeds spawns a background task that re-dials with "
            "exponential backoff. send()/receive() during the outage "
            "block (or buffer, per policy) instead of raising."
        ),
    )

    reconnect_initial_backoff: Duration = timedelta(milliseconds=200)
    """First-retry wait. Doubles each attempt up to
    :attr:`reconnect_max_backoff`."""

    reconnect_max_backoff: Duration = timedelta(seconds=30)
    """Upper bound on the per-attempt delay."""

    reconnect_backoff_jitter: float = Field(
        default=0.25,
        ge=0.0, le=1.0,
        description=(
            "Multiplicative jitter applied to each backoff, in the "
            "range ±jitter. 0.25 = ±25%. Spreads reconnect storms "
            "when many clients lose their peer simultaneously."
        ),
    )

    reconnect_max_attempts: int = Field(
        default=0, ge=0,
        description=(
            "0 = retry forever (default). Non-zero: after this many "
            "consecutive failed attempts, the Client is marked "
            "terminal and subsequent calls raise "
            "ConnectionClosedError."
        ),
    )

    # ---- Resilience: TCP keepalive -------------------------------
    tcp_keepalive: bool = Field(
        default=False,
        description=(
            "Enable SO_KEEPALIVE with tuned intervals so a half-dead "
            "peer (remote crash, cable yanked, NAT idle-out) is "
            "detected in ~60 s instead of hours. Pure OS-level; no "
            "application-layer ping."
        ),
    )

    tcp_keepalive_idle: Duration = timedelta(seconds=30)
    """Seconds of idle before the first keepalive probe is sent."""

    tcp_keepalive_interval: Duration = timedelta(seconds=10)
    """Seconds between keepalive probes once started."""

    tcp_keepalive_count: int = Field(
        default=3, ge=1,
        description=(
            "Number of missed probes before the OS declares the peer "
            "dead."
        ),
    )

    # ---- Resilience: offline buffer ------------------------------
    offline_buffer_capacity: int = Field(
        default=0, ge=0,
        description=(
            "Max messages held while the connection is down. Drained "
            "in FIFO order on reconnect before any new sends. 0 = no "
            "buffering (send during outage raises immediately)."
        ),
    )

    offline_overflow_policy: OfflineOverflow = OfflineOverflow.DROP_NEWEST
    """How ``send()`` behaves when the buffer is full. See
    :class:`OfflineOverflow`."""


# ---- Envelope ----------------------------------------------------


M = TypeVar("M")


class Envelope(BaseModel, Generic[M]):
    """A received message and its surrounding wire header.

    The header carries everything the researcher needs to route on
    — ``device_name``, ``timestamp``, ``type_id``. The body is the
    decoded typed message.

    Kept generic so :meth:`Client.receive(Transform)` returns
    ``Envelope[Transform]`` and IDEs can type-check ``env.body.matrix``.
    """

    model_config = ConfigDict(arbitrary_types_allowed=True)

    header: Header
    body: M


class RawMessage(BaseModel):
    """One IGTL message in its on-the-wire form.

    ``header`` is parsed (callers need it for routing / filtering);
    ``wire`` is the full 58-byte header + body bytes, ready to
    re-send on any transport without repacking. Gateways operate
    on this type, not on decoded :class:`Envelope` instances —
    the point of the gateway pattern is that bytes flow through
    unchanged.

    ``attributes`` is a per-transport free-form key/value map.
    The default v3 framer leaves it empty; a future v4 streaming
    framer would put stream-id / chunk-index here, and middleware
    may add its own keys. See ``spec/ATTRIBUTES.md`` (planned) for
    the shared registry convention.
    """

    model_config = ConfigDict(arbitrary_types_allowed=True)

    header: Header
    wire: bytes
    attributes: dict[str, str] = {}

    @property
    def type_id(self) -> str:
        """Convenience: the wire ``type_id`` string."""
        return self.header.type_id
