"""Conformance oracle — verify wire bytes against the schema-driven codec.

The oracle takes raw wire bytes, runs them through the full codec
pipeline (header parse → CRC verify → schema dispatch → field unpack →
repack → byte comparison), and reports a structured result. It is the
single authoritative check for "is this a valid OpenIGTLink message
that round-trips correctly?"

Usage::

    from oigtl_corpus_tools.codec.oracle import verify_wire_bytes

    result = verify_wire_bytes(wire_data)
    assert result.ok, result.error
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from oigtl_corpus_tools.codec.crc64 import crc64
from oigtl_corpus_tools.codec.header import HEADER_SIZE, unpack_header
from oigtl_corpus_tools.codec.message import load_schema, pack_body, unpack_body


@dataclass
class OracleResult:
    """Outcome of an oracle verification pass."""

    ok: bool
    type_id: str = ""
    device_name: str = ""
    header: dict[str, Any] = field(default_factory=dict)
    body: dict[str, Any] = field(default_factory=dict)
    error: str = ""
    round_trip_ok: bool = False

    def __repr__(self) -> str:
        status = "PASS" if self.ok else "FAIL"
        return f"OracleResult({status}, type={self.type_id!r}, error={self.error!r})"


def verify_wire_bytes(
    data: bytes,
    *,
    check_crc: bool = True,
    check_round_trip: bool = True,
) -> OracleResult:
    """Run the full oracle pipeline on raw wire bytes.

    Steps:
    1. Parse 58-byte header.
    2. Verify body_size fits within data.
    3. Verify CRC (if *check_crc*).
    4. Load schema by type_id.
    5. Unpack body fields.
    6. Repack body and compare bytes (if *check_round_trip*).

    Returns an :class:`OracleResult` — check ``result.ok`` and
    ``result.error`` for the outcome.
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

    # --- Step 4: Schema ---
    try:
        schema = load_schema(header["type"])
    except KeyError:
        result.error = f"no schema for type_id={header['type']!r}"
        return result

    # --- Step 5: Unpack ---
    try:
        body = unpack_body(schema, body_bytes)
    except Exception as exc:
        result.error = f"unpack failed: {exc}"
        return result

    result.body = body

    # --- Step 6: Round-trip ---
    if check_round_trip:
        try:
            repacked = pack_body(schema, body)
        except Exception as exc:
            result.error = f"repack failed: {exc}"
            return result

        if repacked != body_bytes:
            # Find first divergence for diagnostics
            for i, (a, b) in enumerate(zip(body_bytes, repacked)):
                if a != b:
                    result.error = (
                        f"round-trip mismatch at body offset {i}: "
                        f"original=0x{a:02X}, repacked=0x{b:02X} "
                        f"(original {len(body_bytes)}B, repacked {len(repacked)}B)"
                    )
                    return result
            if len(body_bytes) != len(repacked):
                result.error = (
                    f"round-trip length mismatch: "
                    f"original={len(body_bytes)}B, repacked={len(repacked)}B"
                )
                return result

        result.round_trip_ok = True

    result.ok = True
    return result
