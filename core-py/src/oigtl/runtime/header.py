"""Typed wrapper around the 58-byte OpenIGTLink message header.

The reference codec exposes the header as a raw ``dict[str, Any]``;
here we wrap it in a frozen Pydantic model so callers get
attribute access, validation, and IDE help. Symmetric to
``oigtl::runtime::Header`` in core-cpp.
"""

from __future__ import annotations

from pydantic import BaseModel, ConfigDict

from oigtl_corpus_tools.codec.header import HEADER_SIZE
from oigtl_corpus_tools.codec.header import pack_header as _pack_header
from oigtl_corpus_tools.codec.header import unpack_header as _unpack_header

__all__ = ["HEADER_SIZE", "Header", "unpack_header", "pack_header"]


class Header(BaseModel):
    """A parsed 58-byte OpenIGTLink message header."""

    model_config = ConfigDict(frozen=True)

    version: int
    type_id: str
    device_name: str
    timestamp: int
    body_size: int
    crc: int


def unpack_header(data: bytes) -> Header:
    """Parse a 58-byte header into a :class:`Header`.

    Raises :class:`ValueError` if *data* is shorter than 58 bytes
    (kept as ValueError to match the underlying reference codec —
    callers using the oracle layer get a typed
    :class:`~oigtl.runtime.exceptions.ShortBufferError` instead).
    """
    raw = _unpack_header(data)
    return Header(
        version=raw["version"],
        type_id=raw["type"],
        device_name=raw["device_name"],
        timestamp=raw["timestamp"],
        body_size=raw["body_size"],
        crc=raw["crc"],
    )


def pack_header(
    *,
    version: int,
    type_id: str,
    device_name: str,
    timestamp: int,
    body: bytes,
) -> bytes:
    """Build a 58-byte header for *body*. CRC is computed automatically."""
    return _pack_header(
        version=version,
        type_id=type_id,
        device_name=device_name,
        timestamp=timestamp,
        body=body,
    )
