"""Typed options for ``oigtl.net.Client`` / ``Server``.

Kept in a private module so the public API surface (imports from
``oigtl.net.client``) stays compact.

Duration-unit convention
------------------------

We settled on **unit-bearing parameter names** rather than a single
"duration" field with an implicit unit:

- ``timeout=<number>`` → **seconds** (matches ``socket.settimeout``,
  ``asyncio.wait_for``, ``time.sleep``). ``timedelta`` also accepted.
- ``timeout_ms=<int>`` → **milliseconds**, integer. Idiomatic when
  tuning network budgets.

Every duration field on :class:`ClientOptions` accepts both spellings
— e.g. ``connect_timeout=2.5`` is equivalent to
``connect_timeout_ms=2500``. Setting both on the same construction
raises :class:`ValueError`.

(Historical note: before v0.4.0 bare numbers were interpreted as
milliseconds. That conflicted with the user-facing NET_GUIDE and
with stdlib convention; tests were already passing ``timeout=2``
expecting seconds. The switch landed as a breaking change — callers
relying on the old behaviour should update to the ``_ms`` variant.)
"""

from __future__ import annotations

from datetime import timedelta
from enum import Enum
from typing import Annotated, Any

from pydantic import (
    BaseModel,
    BeforeValidator,
    ConfigDict,
    Field,
    model_validator,
)

from oigtl.runtime.envelope import Envelope, RawMessage

__all__ = [
    "ClientOptions",
    "Envelope",
    "OfflineOverflow",
    "RawMessage",
    "as_timedelta",
    "as_timedelta_ms",
    "resolve_timeout",
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
    - ``int`` / ``float`` — interpreted as **seconds** (SI base unit,
      stdlib convention: ``socket.settimeout``, ``asyncio.wait_for``,
      ``time.sleep``).
    - ``None`` — passed through; the field's type must include it.

    For a bare count of milliseconds, use the ``_ms`` variant of the
    field (e.g. ``connect_timeout_ms=500``) or call
    :func:`as_timedelta_ms` directly.
    """
    if value is None or isinstance(value, timedelta):
        return value  # type: ignore[return-value]
    if isinstance(value, (int, float)):
        return timedelta(seconds=value)
    raise TypeError(
        f"duration fields accept timedelta or int/float seconds, "
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
    """Coerce an inline ``timeout=`` argument (seconds or timedelta).

    Exposed because :meth:`Client.receive` and friends accept an
    inline ``timeout=`` argument that should honour the same rule as
    the option-struct fields: bare numbers are seconds.
    """
    return _coerce_to_timedelta(value)


def as_timedelta_ms(
    value: timedelta | int | float | None,
) -> timedelta | None:
    """Coerce an inline ``timeout_ms=`` argument.

    Counterpart to :func:`as_timedelta` for callers writing
    ``timeout_ms=500``. ``timedelta`` and ``None`` pass through;
    bare numbers are interpreted as milliseconds.
    """
    if value is None or isinstance(value, timedelta):
        return value
    if isinstance(value, (int, float)):
        return timedelta(milliseconds=value)
    raise TypeError(
        f"duration _ms fields accept timedelta or int/float ms, "
        f"got {type(value).__name__}"
    )


def resolve_timeout(
    timeout: timedelta | int | float | None,
    timeout_ms: timedelta | int | float | None,
) -> timedelta | None:
    """Pick one of ``timeout`` (seconds) or ``timeout_ms`` (ms).

    Helper for the inline ``receive`` / ``receive_any`` APIs that
    accept both spellings. Setting both raises ``ValueError``.
    Returns ``None`` if neither is set — callers fall back to the
    option-struct default.
    """
    if timeout is not None and timeout_ms is not None:
        raise ValueError(
            "pass timeout= (seconds) OR timeout_ms= (milliseconds), "
            "not both"
        )
    if timeout_ms is not None:
        return as_timedelta_ms(timeout_ms)
    return as_timedelta(timeout)


# Set of ClientOptions duration-field names. Kept in sync by hand —
# the pre-validator below uses it to translate "<name>_ms" → "<name>".
_DURATION_FIELDS = frozenset({
    "connect_timeout",
    "receive_timeout",
    "send_timeout",
    "reconnect_initial_backoff",
    "reconnect_max_backoff",
    "tcp_keepalive_idle",
    "tcp_keepalive_interval",
})


class ClientOptions(BaseModel):
    """Configuration for :class:`~oigtl.net.client.Client`.

    Mirrors the capability of ``oigtl::ClientOptions`` in
    ``core-cpp/include/oigtl/client.hpp``. Phase 2 only consumes the
    connection/send/receive fields; resilience fields land in
    Phase 4.

    Duration fields accept either a :class:`~datetime.timedelta` or
    a bare number of **seconds**:
    ``ClientOptions(connect_timeout=2.5)``. For a bare count of
    milliseconds, pass the ``_ms`` companion:
    ``ClientOptions(connect_timeout_ms=2500)`` (equivalent). Setting
    both the base name and its ``_ms`` companion on the same call is
    an error.
    """

    model_config = ConfigDict(
        arbitrary_types_allowed=True,
        # Reject keyword arguments we don't recognise. The _ms
        # variants are handled in a pre-validator below, so they're
        # translated away before the model hits this check.
        extra="forbid",
    )

    @model_validator(mode="before")
    @classmethod
    def _translate_ms_fields(cls, data: Any) -> Any:
        """Translate ``<name>_ms=<int>`` into a timedelta on ``<name>``.

        Raises if the caller supplies both the canonical field and
        its ``_ms`` companion on the same construction — that would
        be ambiguous.
        """
        if not isinstance(data, dict):
            return data
        for field in _DURATION_FIELDS:
            ms_key = f"{field}_ms"
            if ms_key not in data:
                continue
            if field in data:
                raise ValueError(
                    f"{field} and {ms_key} are mutually exclusive "
                    f"(set one or the other, not both)"
                )
            ms_value = data.pop(ms_key)
            # None propagates unchanged (same semantics as None on
            # the canonical field: "no budget").
            if ms_value is None:
                data[field] = None
            elif isinstance(ms_value, timedelta):
                # Accept timedelta on the _ms variant too — it would
                # be strange to reject it. Just pass through.
                data[field] = ms_value
            elif isinstance(ms_value, (int, float)):
                data[field] = timedelta(milliseconds=ms_value)
            else:
                raise TypeError(
                    f"{ms_key} accepts int/float/timedelta/None, "
                    f"got {type(ms_value).__name__}"
                )
        return data

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
    """Time budget for the initial TCP connect. ``None`` = no limit.
    Bare numbers are seconds; use ``connect_timeout_ms`` for ms."""

    receive_timeout: OptionalDuration = None
    """Default wall-clock budget for ``receive()`` / ``receive_any()``
    when the caller doesn't pass a per-call timeout. ``None`` =
    block forever. Bare numbers are seconds; use
    ``receive_timeout_ms`` for ms."""

    send_timeout: OptionalDuration = None
    """Budget for a blocked ``send()`` when the offline buffer is full
    and :attr:`offline_overflow_policy` is ``BLOCK``. ``None`` = wait
    forever. Has no effect on other policies. Bare numbers are
    seconds; use ``send_timeout_ms`` for ms."""

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
    :attr:`reconnect_max_backoff`. Bare numbers are seconds; use
    ``reconnect_initial_backoff_ms`` for ms."""

    reconnect_max_backoff: Duration = timedelta(seconds=30)
    """Upper bound on the per-attempt delay. Bare numbers are
    seconds; use ``reconnect_max_backoff_ms`` for ms."""

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
    """Seconds of idle before the first keepalive probe is sent.
    Bare numbers are seconds; use ``tcp_keepalive_idle_ms`` for ms."""

    tcp_keepalive_interval: Duration = timedelta(seconds=10)
    """Seconds between keepalive probes once started. Bare numbers
    are seconds; use ``tcp_keepalive_interval_ms`` for ms."""

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


# :class:`Envelope` and :class:`RawMessage` previously lived in this
# module; they moved to :mod:`oigtl.runtime.envelope` so the pure
# codec can return them without pulling in the whole transport
# layer. They are re-exported above for backwards compatibility —
# ``from oigtl.net._options import Envelope`` continues to work.
