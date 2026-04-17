"""Conformance oracle — verify wire bytes against the schema-driven codec.

The oracle takes raw wire bytes and runs them through the full codec
pipeline:

    header parse → CRC verify → body framing split →
    schema dispatch → field unpack → repack → byte comparison

It is the single authoritative check for "is this a valid OpenIGTLink
message that round-trips correctly?"

For v2+ messages, the body consists of four concatenated regions:

    [ extended_header ][ content ][ metadata_index ][ metadata_body ]

The schema under ``spec/schemas/`` describes only the content region;
the oracle preserves the framing regions as opaque bytes for the
round-trip comparison. When the header declares version >= 2, the
oracle parses the extended header to locate region boundaries; for
v1 messages, the entire body is content.

Usage::

    from oigtl_corpus_tools.codec.oracle import verify_wire_bytes

    result = verify_wire_bytes(wire_data)
    assert result.ok, result.error
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from typing import Any

from oigtl_corpus_tools.codec.crc64 import crc64
from oigtl_corpus_tools.codec.header import HEADER_SIZE, unpack_header
from oigtl_corpus_tools.codec.message import load_schema, pack_body, unpack_body
from oigtl_corpus_tools.codec.policy import check_body_size_set


# Minimum extended-header size (4 fields: u16, u16, u32, u32 = 12 bytes).
_EXT_HEADER_MIN_SIZE = 12


@dataclass
class OracleResult:
    """Outcome of an oracle verification pass."""

    ok: bool
    type_id: str = ""
    device_name: str = ""
    header: dict[str, Any] = field(default_factory=dict)
    extended_header: dict[str, Any] | None = None
    body: dict[str, Any] = field(default_factory=dict)
    metadata: list[tuple[str, int, bytes]] = field(default_factory=list)
    error: str = ""
    round_trip_ok: bool = False

    def __repr__(self) -> str:
        status = "PASS" if self.ok else "FAIL"
        return f"OracleResult({status}, type={self.type_id!r}, error={self.error!r})"


# ---------------------------------------------------------------------------
# Extended header + metadata framing
# ---------------------------------------------------------------------------


def _parse_extended_header(
    body: bytes,
) -> tuple[dict[str, Any], bytes]:
    """Parse the v2/v3 extended header at the start of *body*.

    Returns ``(ext_header_dict, ext_header_bytes)``. The ``_dict`` form
    exposes the four declared fields; the raw bytes are preserved for
    round-trip (the extended header may contain reserved bytes past
    the declared size).

    Raises :class:`ValueError` on malformed extended header.
    """
    if len(body) < _EXT_HEADER_MIN_SIZE:
        raise ValueError(
            f"body too short for extended header: {len(body)} < {_EXT_HEADER_MIN_SIZE}"
        )
    ext_header_size, metadata_header_size, metadata_size, message_id = (
        struct.unpack_from(">HHII", body, 0)
    )
    if ext_header_size < _EXT_HEADER_MIN_SIZE:
        raise ValueError(
            f"extended header size {ext_header_size} < minimum {_EXT_HEADER_MIN_SIZE}"
        )
    if ext_header_size > len(body):
        raise ValueError(
            f"extended header size {ext_header_size} exceeds body size {len(body)}"
        )
    declared = {
        "ext_header_size": ext_header_size,
        "metadata_header_size": metadata_header_size,
        "metadata_size": metadata_size,
        "message_id": message_id,
    }
    return declared, bytes(body[:ext_header_size])


def _parse_metadata(
    metadata_region: bytes,
    metadata_header_size: int,
    metadata_size: int,
) -> list[tuple[str, int, bytes]]:
    """Parse the metadata index + body into a list of ``(key, encoding, value)`` tuples.

    Returns a list of entries. Raises :class:`ValueError` on malformed
    metadata (size mismatch, declared key/value sums exceed region).
    """
    if metadata_header_size == 0 and metadata_size == 0:
        return []
    if len(metadata_region) < metadata_header_size + metadata_size:
        raise ValueError(
            f"metadata region shorter than declared: "
            f"have {len(metadata_region)}, need "
            f"{metadata_header_size + metadata_size}"
        )
    if metadata_header_size < 2:
        raise ValueError(
            f"metadata_header_size {metadata_header_size} < 2 (count field)"
        )

    # Parse index section
    count = int.from_bytes(metadata_region[0:2], "big")
    expected_header_bytes = 2 + count * 8
    if metadata_header_size < expected_header_bytes:
        raise ValueError(
            f"metadata_header_size {metadata_header_size} too small "
            f"for {count} entries (need {expected_header_bytes})"
        )

    entries: list[tuple[int, int, int]] = []
    offset = 2
    for _ in range(count):
        key_size, value_encoding, value_size = struct.unpack_from(
            ">HHI", metadata_region, offset
        )
        entries.append((key_size, value_encoding, value_size))
        offset += 8

    # Parse body section
    body_offset = metadata_header_size
    result: list[tuple[str, int, bytes]] = []
    for key_size, value_encoding, value_size in entries:
        key_bytes = metadata_region[body_offset : body_offset + key_size]
        body_offset += key_size
        value_bytes = metadata_region[body_offset : body_offset + value_size]
        body_offset += value_size
        result.append((key_bytes.decode("utf-8", errors="replace"), value_encoding, bytes(value_bytes)))

    return result


# ---------------------------------------------------------------------------
# Main oracle entry point
# ---------------------------------------------------------------------------


def verify_wire_bytes(
    data: bytes,
    *,
    check_crc: bool = True,
    check_round_trip: bool = True,
) -> OracleResult:
    """Run the full oracle pipeline on raw wire bytes.

    Steps:
    1. Parse 58-byte header.
    2. Verify body_size fits within *data*.
    3. Verify CRC (if *check_crc*).
    4. If version >= 2: slice body into [ext_header, content, metadata]
       and parse the extended header. Otherwise body IS content.
    5. Load schema by type_id, unpack content.
    6. Repack content and compare all regions byte-for-byte (if
       *check_round_trip*).

    Returns an :class:`OracleResult`.
    """
    result = OracleResult(ok=False)

    # --- Step 1: Header ---
    if len(data) < HEADER_SIZE:
        result.error = f"too short for header: {len(data)} < {HEADER_SIZE}"
        return result

    try:
        header = unpack_header(data)
    except Exception as exc:
        result.error = f"header parse failed: {exc}"
        return result

    result.header = header
    result.type_id = header["type"]
    result.device_name = header["device_name"]

    # --- Step 2: Body bounds ---
    body_start = HEADER_SIZE
    body_end = HEADER_SIZE + header["body_size"]
    if len(data) < body_end:
        result.error = (
            f"truncated: header declares body_size={header['body_size']}, "
            f"but only {len(data) - HEADER_SIZE} body bytes available"
        )
        return result

    body_bytes = data[body_start:body_end]

    # --- Step 3: CRC ---
    if check_crc:
        computed_crc = crc64(body_bytes)
        if computed_crc != header["crc"]:
            result.error = (
                f"CRC mismatch: header=0x{header['crc']:016X}, "
                f"computed=0x{computed_crc:016X}"
            )
            return result

    # --- Step 4: Body framing split (v2+) ---
    ext_header_bytes = b""
    metadata_bytes = b""
    content_bytes = body_bytes

    if header["version"] >= 2:
        try:
            ext_header_decl, ext_header_bytes = _parse_extended_header(body_bytes)
        except Exception as exc:
            result.error = f"extended header parse failed: {exc}"
            return result
        result.extended_header = ext_header_decl

        metadata_total = (
            ext_header_decl["metadata_header_size"]
            + ext_header_decl["metadata_size"]
        )
        content_start = ext_header_decl["ext_header_size"]
        content_end = len(body_bytes) - metadata_total
        if content_end < content_start:
            result.error = (
                f"framing inconsistent: ext_header={ext_header_decl['ext_header_size']}, "
                f"metadata_total={metadata_total}, body_size={len(body_bytes)}"
            )
            return result
        content_bytes = body_bytes[content_start:content_end]
        metadata_bytes = body_bytes[content_end:]

        # Parse metadata entries (non-fatal: surface as data, not error)
        try:
            result.metadata = _parse_metadata(
                metadata_bytes,
                ext_header_decl["metadata_header_size"],
                ext_header_decl["metadata_size"],
            )
        except Exception as exc:
            result.error = f"metadata parse failed: {exc}"
            return result

    # --- Step 5: Schema + content unpack ---
    try:
        schema = load_schema(header["type"])
    except KeyError:
        result.error = f"no schema for type_id={header['type']!r}"
        return result

    # Spec-level whitelist (e.g. POSITION body ∈ {12, 24, 28}). Reject
    # before field walking so the error surfaces as MALFORMED rather
    # than leaking through as a truncation of some interior field.
    try:
        check_body_size_set(schema, len(content_bytes))
    except ValueError as exc:
        result.error = str(exc)
        return result

    try:
        body = unpack_body(schema, content_bytes)
    except Exception as exc:
        result.error = f"unpack failed: {exc}"
        return result

    result.body = body

    # --- Step 6: Round-trip ---
    if check_round_trip:
        try:
            repacked_content = pack_body(schema, body)
        except Exception as exc:
            result.error = f"repack failed: {exc}"
            return result

        if repacked_content != content_bytes:
            for i, (a, b) in enumerate(zip(content_bytes, repacked_content)):
                if a != b:
                    result.error = (
                        f"round-trip mismatch at content offset {i}: "
                        f"original=0x{a:02X}, repacked=0x{b:02X} "
                        f"(original {len(content_bytes)}B, repacked "
                        f"{len(repacked_content)}B)"
                    )
                    return result
            if len(content_bytes) != len(repacked_content):
                result.error = (
                    f"round-trip length mismatch: "
                    f"original={len(content_bytes)}B, "
                    f"repacked={len(repacked_content)}B"
                )
                return result

        # Verify the full body round-trips (ext_header + content + metadata)
        repacked_body = ext_header_bytes + repacked_content + metadata_bytes
        if repacked_body != body_bytes:
            result.error = (
                f"full-body round-trip mismatch "
                f"({len(body_bytes)}B original, {len(repacked_body)}B repacked)"
            )
            return result

        result.round_trip_ok = True

    result.ok = True
    return result
