"""v3 framer — 58-byte header + body_size body.

Mirrors ``oigtl::transport::make_v3_framer`` in
``core-cpp/src/transport/framer_v3.cpp``.

A framer does two jobs:

1. Peel one :class:`Incoming` off the front of an accumulating
   byte buffer. Returns ``None`` when the buffer doesn't yet carry
   a full message (short read, not an error).
2. Wrap an outbound packed message for the wire. For v3 this is
   identity — our codec already emits framed bytes.

The default framer is the only one we ship today; a future v4
streaming / multiplexed framer is a different impl of the same
interface. Network transports stay identical.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Protocol

from oigtl.runtime.exceptions import (
    CrcMismatchError,
    ProtocolError,
    ShortBufferError,
)
from oigtl.runtime.header import HEADER_SIZE, Header, unpack_header
from oigtl_corpus_tools.codec.crc64 import crc64

from oigtl.net.errors import FramingError

__all__ = [
    "FramerMetadata",
    "Framer",
    "Incoming",
    "V3Framer",
    "make_v3_framer",
]


@dataclass
class FramerMetadata:
    """Optional per-message metadata emitted by a framer.

    The default v3 framer never sets this. A future v4 chunked
    framer would carry stream-id / chunk-index here.
    """

    framer_name: str = ""
    attributes: list[tuple[str, str]] = field(default_factory=list)


@dataclass
class Incoming:
    """One parsed wire message.

    ``header`` + ``body`` are a unit; together they reconstitute the
    bytes on the wire except for any framer-added envelope (stripped
    by :meth:`Framer.try_parse`). ``body`` is the body alone, without
    the 58-byte header — matching the codec's ``unpack_*`` signature.
    """

    header: Header
    body: bytes
    metadata: FramerMetadata | None = None


class Framer(Protocol):
    """Protocol for wire-format framers.

    ``try_parse`` consumes a prefix of ``buffer`` on success (using a
    mutable :class:`bytearray` so callers don't pay O(N²) copies in
    an accumulating receive loop). Returns ``None`` on short read —
    the caller keeps accumulating. Raises on malformed bytes.
    """

    def try_parse(self, buffer: bytearray) -> Incoming | None: ...

    def frame(self, wire: bytes) -> bytes: ...

    @property
    def name(self) -> str: ...


class V3Framer:
    """Default v3 framer: 58-byte header + body_size body, no envelope.

    ``max_body_size`` of 0 means no additional cap (body_size is still
    bounded by its 64-bit wire field). Non-zero means
    :meth:`try_parse` raises :class:`~oigtl.net.errors.FramingError`
    if the header announces ``body_size > max_body_size``, BEFORE any
    body bytes are buffered — a pre-parse DoS defence.
    """

    def __init__(self, max_body_size: int = 0) -> None:
        if max_body_size < 0:
            raise ValueError("max_body_size must be >= 0")
        self._max_body_size = max_body_size

    @property
    def name(self) -> str:
        return "v3"

    def try_parse(self, buffer: bytearray) -> Incoming | None:
        if len(buffer) < HEADER_SIZE:
            return None

        # Parse the header first. The codec raises ValueError on a
        # bogus version or short buffer; surface as ProtocolError.
        try:
            header = unpack_header(bytes(buffer[:HEADER_SIZE]))
        except ValueError as e:
            raise ProtocolError(str(e)) from e

        # Per-policy pre-parse cap. Enforced BEFORE the body-bytes
        # availability check so a peer announcing a huge body_size is
        # rejected immediately, not after waiting for bytes.
        if 0 < self._max_body_size < header.body_size:
            raise FramingError(
                f"body_size {header.body_size} exceeds configured "
                f"max_message_size {self._max_body_size}"
            )

        total = HEADER_SIZE + header.body_size
        if len(buffer) < total:
            return None

        body = bytes(buffer[HEADER_SIZE:total])

        # Verify CRC now — catches wire corruption or injection. A
        # malformed CRC is a receive-side error surfaced to the caller.
        computed = crc64(body)
        if computed != header.crc:
            raise CrcMismatchError(
                f"header crc=0x{header.crc:016x} body crc=0x{computed:016x}"
            )

        # Consume the prefix. O(N) on buffer tail; callers hand us
        # their own scratch buffer and message rates are modest.
        del buffer[:total]

        return Incoming(header=header, body=body, metadata=None)

    def frame(self, wire: bytes) -> bytes:
        # v3: codec output is already framed.
        return wire


def make_v3_framer(max_body_size: int = 0) -> V3Framer:
    """Construct the default v3 framer (see :class:`V3Framer`)."""
    return V3Framer(max_body_size=max_body_size)


# Re-export for consumers who want the split constant alongside the
# framer. ``ShortBufferError`` isn't raised from the framer — short
# reads return ``None`` — but downstream transport code that wraps
# the framer may surface it.
__all__ += ["HEADER_SIZE", "ShortBufferError"]
