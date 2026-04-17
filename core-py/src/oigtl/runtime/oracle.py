"""Typed conformance oracle.

A thin Pydantic-typed wrapper around the reference oracle in
:mod:`oigtl_corpus_tools.codec.oracle`. Mirrors the C++
``oigtl::runtime::oracle::verify_wire_bytes`` surface — same fields,
same field names, same semantics. Translates the underlying
codec's plain-string errors into the typed exception hierarchy
(:mod:`oigtl.runtime.exceptions`) for callers that prefer
``except CrcMismatchError`` to string-matching.
"""

from __future__ import annotations

from typing import Any, Optional

from pydantic import BaseModel, ConfigDict

from oigtl.runtime.header import Header

from oigtl_corpus_tools.codec.oracle import verify_wire_bytes as _ref_verify


__all__ = [
    "ExtendedHeader",
    "MetadataEntry",
    "VerifyResult",
    "verify_wire_bytes",
]


class ExtendedHeader(BaseModel):
    """The 12-byte v3 extended header."""
    model_config = ConfigDict(frozen=True)

    ext_header_size: int
    metadata_header_size: int
    metadata_size: int
    message_id: int


class MetadataEntry(BaseModel):
    """One entry in the v3 metadata block."""
    model_config = ConfigDict(frozen=True)

    key: str
    value_encoding: int
    value: bytes


class VerifyResult(BaseModel):
    """Outcome of an oracle verification pass."""
    model_config = ConfigDict(arbitrary_types_allowed=True)

    ok: bool = False
    error: str = ""
    header: Optional[Header] = None
    extended_header: Optional[ExtendedHeader] = None
    metadata: list[MetadataEntry] = []
    # Body values as a dict (matches reference codec). The typed
    # parse_message helper in :mod:`oigtl.messages` returns a
    # typed instance instead.
    body: dict[str, Any] = {}
    round_trip_ok: bool = False


def verify_wire_bytes(
    data: bytes,
    *,
    check_crc: bool = True,
    check_round_trip: bool = True,
) -> VerifyResult:
    """Run the full oracle pipeline on raw wire bytes.

    Identical semantics to
    :func:`oigtl_corpus_tools.codec.oracle.verify_wire_bytes` —
    parses the 58-byte header, verifies CRC, slices framing
    regions, decodes metadata, looks up the schema by type_id,
    unpacks + repacks the body content and confirms byte-equality.

    Returns a :class:`VerifyResult`. Does not raise on protocol
    failures (these surface as ``ok=False``) — only raises on
    programmer error.
    """
    raw = _ref_verify(
        data, check_crc=check_crc, check_round_trip=check_round_trip
    )

    out = VerifyResult(ok=raw.ok, error=raw.error)
    if raw.header:
        out.header = Header(
            version=raw.header["version"],
            type_id=raw.header["type"],
            device_name=raw.header["device_name"],
            timestamp=raw.header["timestamp"],
            body_size=raw.header["body_size"],
            crc=raw.header["crc"],
        )
    if raw.extended_header:
        out.extended_header = ExtendedHeader(**raw.extended_header)
    out.metadata = [
        MetadataEntry(key=k, value_encoding=enc, value=v)
        for k, enc, v in raw.metadata
    ]
    out.body = raw.body
    out.round_trip_ok = raw.round_trip_ok
    return out
