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
    "typed_verify_wire_bytes",
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


# ---------------------------------------------------------------------------
# Typed oracle — exercises the generated class path, not the dict codec.
# ---------------------------------------------------------------------------


def typed_verify_wire_bytes(
    data: bytes,
    *,
    check_crc: bool = True,
    check_round_trip: bool = True,
) -> VerifyResult:
    """Run the oracle pipeline through the TYPED class layer.

    Differs from :func:`verify_wire_bytes` in step 5/6: where the
    reference oracle round-trips via the dict-based codec, this one
    looks up the ``type_id`` in the generated message registry and
    calls ``Class.unpack(content_bytes)`` / ``instance.pack()``.

    This exercises the numpy / array.array coercion paths in
    :mod:`oigtl.runtime.arrays` and the Pydantic field validators
    that the reference codec bypasses. Intended primarily for the
    differential fuzzer — toggle
    ``OIGTL_NO_NUMPY=1`` to run the array.array fallback path.
    """
    # Reuse the reference oracle for framing + CRC (those layers
    # don't have a typed equivalent and are already exhaustively
    # tested). If the reference oracle fails, so does this one —
    # the typed path wouldn't even see the content bytes.
    raw = _ref_verify(data, check_crc=check_crc, check_round_trip=False)

    out = VerifyResult(ok=False, error=raw.error)
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

    if not raw.ok:
        return out

    # Re-derive content_bytes — the reference oracle doesn't expose
    # them but we can reconstruct from the header + framing decisions.
    # Import here to avoid a module-level circular dep.
    from oigtl.messages import REGISTRY
    from oigtl_corpus_tools.codec.header import HEADER_SIZE

    body_size = int(raw.header["body_size"])
    body = data[HEADER_SIZE:HEADER_SIZE + body_size]
    if raw.header["version"] >= 2 and raw.extended_header is not None:
        eh = raw.extended_header
        content_start = eh["ext_header_size"]
        content_end = len(body) - eh["metadata_header_size"] - eh["metadata_size"]
        content_bytes = body[content_start:content_end]
    else:
        content_bytes = body

    type_id = raw.header["type"]
    cls = REGISTRY.get(type_id)
    if cls is None:
        out.error = f"no typed class registered for type_id={type_id!r}"
        return out

    try:
        instance = cls.unpack(content_bytes)
    except Exception as exc:
        out.error = f"typed unpack failed for {type_id}: {exc}"
        return out

    if check_round_trip:
        try:
            repacked = instance.pack()
        except Exception as exc:
            out.error = f"typed pack failed for {type_id}: {exc}"
            return out
        if bytes(repacked) != bytes(content_bytes):
            # First differing byte, same shape as the reference oracle
            # so disagreement reports stay comparable.
            for i, (a, b) in enumerate(zip(content_bytes, repacked)):
                if a != b:
                    out.error = (
                        f"typed round-trip mismatch for {type_id} at content "
                        f"offset {i}: original=0x{a:02X}, repacked=0x{b:02X} "
                        f"({len(content_bytes)}B in, {len(repacked)}B out)"
                    )
                    return out
            if len(content_bytes) != len(repacked):
                out.error = (
                    f"typed round-trip length mismatch for {type_id}: "
                    f"{len(content_bytes)}B in, {len(repacked)}B out"
                )
                return out
        out.round_trip_ok = True
    else:
        out.round_trip_ok = False

    out.ok = True
    out.body = raw.body
    return out
