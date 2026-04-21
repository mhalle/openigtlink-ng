"""Transport-independent container types for decoded and raw messages.

Lives in :mod:`oigtl.runtime` because the :class:`Envelope` is the
codec's public return type, and the codec sits below every
transport. Moving these types above the transport layer would create
a circular dependency with :mod:`oigtl.codec` (codec needs to return
Envelope; transports need codec; transports can therefore not define
Envelope).

The classes are re-exported from :mod:`oigtl.net._options` for
backwards-compatible imports — ``from oigtl.net._options import
Envelope`` still works.
"""

from __future__ import annotations

from typing import Generic, TypeVar

from pydantic import BaseModel, ConfigDict

from oigtl.runtime.header import Header

__all__ = ["Envelope", "RawMessage"]


M = TypeVar("M")


class Envelope(BaseModel, Generic[M]):
    """A received message and its surrounding wire header.

    The header carries everything the researcher needs to route on
    — ``device_name``, ``timestamp``, ``type_id``. The body is the
    decoded typed message.

    Kept generic so :meth:`Client.receive(Transform)` returns
    ``Envelope[Transform]`` and IDEs can type-check ``env.body.matrix``.

    "Envelope" follows the SOAP-style convention — a container for
    the whole parsed message (header + body) rather than just the
    outer routing wrapper (which would be the SMTP/AMQP sense).
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
