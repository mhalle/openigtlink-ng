"""High-level typed dispatch — parse wire bytes into a typed message.

The codegen-emitted ``__init__.py`` provides ``REGISTRY`` for
type_id → class lookup; this module wraps that with a one-call
entry point that does the framing parse + content slice + dispatch
in one shot. Mirrors the role of the C++
``oracle::verify_wire_bytes(..., registry)`` overload.
"""

from __future__ import annotations

import struct
from typing import Optional

from pydantic import BaseModel

from oigtl.runtime.exceptions import (
    CrcMismatchError,
    MalformedMessageError,
    ProtocolError,
    UnknownMessageTypeError,
)

from oigtl_corpus_tools.codec.header import HEADER_SIZE, unpack_header
from oigtl_corpus_tools.codec.crc64 import crc64


__all__ = ["parse_message", "extract_content_bytes"]


def extract_content_bytes(wire: bytes) -> tuple[str, bytes]:
    """Return ``(type_id, content_bytes)`` for the given wire message.

    Handles v1 (body == content) and v2/v3 (body has extended header
    + content + metadata regions). Does NOT verify CRC — callers
    that need that should use :func:`parse_message` or the typed
    oracle.
    """
    if len(wire) < HEADER_SIZE:
        raise MalformedMessageError(
            f"too short for header: {len(wire)} < {HEADER_SIZE}"
        )
    header = unpack_header(wire)
    body_end = HEADER_SIZE + header["body_size"]
    if len(wire) < body_end:
        raise MalformedMessageError(
            f"truncated: header declares body_size={header['body_size']}, "
            f"but only {len(wire) - HEADER_SIZE} body bytes available"
        )
    body = wire[HEADER_SIZE:body_end]

    if header["version"] < 2:
        return header["type"], body

    # v2/v3: parse the 12-byte ext header to find content slice.
    if len(body) < 12:
        raise MalformedMessageError(
            f"body too short for extended header: {len(body)}"
        )
    ext_header_size, mh, ms, _msg_id = struct.unpack_from(">HHII", body, 0)
    if ext_header_size < 12 or ext_header_size > len(body):
        raise MalformedMessageError(
            f"bad extended_header_size {ext_header_size}"
        )
    metadata_total = mh + ms
    if metadata_total > len(body) - ext_header_size:
        raise MalformedMessageError(
            f"metadata regions overrun body"
        )
    return header["type"], body[ext_header_size:len(body) - metadata_total]


def parse_message(
    wire: bytes,
    *,
    registry: Optional[dict[str, type]] = None,
    check_crc: bool = True,
) -> BaseModel:
    """Parse wire bytes into the appropriate typed message instance.

    Combines header parse + CRC verify + framing slice + dispatch
    via *registry* (defaults to the all-types REGISTRY) into one
    call.

    Raises:
        ShortBufferError / MalformedMessageError on framing failure
        CrcMismatchError when *check_crc* is True and CRC fails
        UnknownMessageTypeError if no codec is registered for the
            wire type_id (extend with your own classes by passing a
            registry dict)
    """
    if registry is None:
        # Imported lazily to avoid a cycle (messages/__init__ imports
        # this module via core-py's exports).
        from oigtl.messages import REGISTRY as registry  # type: ignore

    type_id, content = extract_content_bytes(wire)

    if check_crc:
        # Re-derive body for the CRC computation (extract_content
        # already validated body length).
        header = unpack_header(wire)
        body = wire[HEADER_SIZE:HEADER_SIZE + header["body_size"]]
        computed = crc64(body)
        if computed != header["crc"]:
            raise CrcMismatchError(
                f"CRC mismatch: header=0x{header['crc']:016X}, "
                f"computed=0x{computed:016X}"
            )

    cls = registry.get(type_id)
    if cls is None:
        raise UnknownMessageTypeError(
            f"no codec registered for type_id={type_id!r}"
        )
    return cls.unpack(content)
