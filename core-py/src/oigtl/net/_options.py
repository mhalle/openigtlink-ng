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
from typing import Annotated, Generic, TypeVar

from pydantic import BaseModel, BeforeValidator, ConfigDict, Field

from oigtl.runtime.header import Header

__all__ = [
    "ClientOptions",
    "Envelope",
    "as_timedelta",
]


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
