"""Deterministic negative-corpus generator.

Each entry is a byte sequence that every codec must reject — it
violates the spec in a specific, documented way. Entries are
the *dual* of the upstream positive fixtures: together they pin
behaviour on both sides of the accept/reject boundary.

The generator is a module of named builder functions. A single
driver iterates them, writes each to ``spec/corpus/negative/<path>``,
and emits ``spec/corpus/negative/index.json`` describing the full
set for downstream test harnesses.

## Error-class vocabulary

The ``error_class`` field uses a small language-independent enum:

- ``SHORT_BUFFER``   — a read would have exceeded available bytes
- ``CRC_MISMATCH``   — header CRC doesn't match body CRC
- ``MALFORMED``      — structurally invalid (size mismatches, bad framing)
- ``UNKNOWN_TYPE``   — wire type_id isn't in the dispatch registry

Each codec's per-codec test maps these to its own exception class.
The test contract is: *base class* ``ProtocolError`` MUST be raised;
subclass preferences are documented but not enforced across
languages because error taxonomies diverged slightly (core-ts has
``HeaderParseError`` / ``BodyDecodeError`` while core-cpp has
``ShortBufferError`` / ``MalformedMessageError``). Both partitions
are reasonable; we don't relitigate.
"""

from __future__ import annotations

import dataclasses
import json
import struct
from pathlib import Path
from typing import Callable

from oigtl_corpus_tools.codec.crc64 import crc64
from oigtl_corpus_tools.codec.header import HEADER_SIZE
from oigtl_corpus_tools.codec.test_vectors import UPSTREAM_VECTORS


NEGATIVE_CORPUS_FORMAT_VERSION = 1


@dataclasses.dataclass
class NegativeEntry:
    """One row in the corpus index."""
    name: str
    path: str                  # relative to spec/corpus/negative/
    description: str
    error_class: str           # one of the enum values above
    spec_reference: str        # section + doc this test codifies
    data: bytes                # the wire bytes themselves
    known_issue: str = ""      # when non-empty, the codec(s) listed in
                               # currently_accepted_by do NOT yet reject
                               # this entry (bug, tracked as a followup).
    currently_accepted_by: tuple[str, ...] = ()  # subset of {"py-ref",
                               # "py", "cpp", "ts"}


# ---------------------------------------------------------------------------
# Building blocks — a header with a chosen body size and (optionally) valid CRC
# ---------------------------------------------------------------------------

def _pack_header(
    *,
    version: int = 1,
    type_id: str = "TRANSFORM",
    device_name: str = "DeviceName",
    timestamp: int = 0x00000000499602D4,
    body: bytes = b"",
    body_size_override: int | None = None,
    crc_override: int | None = None,
) -> bytes:
    """Pack a 58-byte header with sensible defaults."""
    tid = type_id.encode("ascii").ljust(12, b"\x00")[:12]
    dev = device_name.encode("ascii").ljust(20, b"\x00")[:20]
    body_size = body_size_override if body_size_override is not None else len(body)
    crc_value = crc_override if crc_override is not None else crc64(body)
    return (
        struct.pack(">H", version)
        + tid
        + dev
        + struct.pack(">Q", timestamp)
        + struct.pack(">Q", body_size)
        + struct.pack(">Q", crc_value)
    )


def _valid_transform_body() -> bytes:
    """A valid 48-byte TRANSFORM content."""
    # Identity-ish matrix in float32
    return struct.pack(">12f", 1.0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0)


# ---------------------------------------------------------------------------
# Builders — one per negative corpus entry
# ---------------------------------------------------------------------------


Builder = Callable[[], bytes]


def _framing_truncated_header_42() -> bytes:
    """42 bytes — less than the 58-byte header."""
    return b"\x00" * 42


def _framing_truncated_header_57() -> bytes:
    """57 bytes — one byte short of the header."""
    return b"\x00" * 57


def _framing_body_size_overflow() -> bytes:
    """Header declares body_size=2^62; receiver can't possibly have it."""
    return _pack_header(body=b"", body_size_override=1 << 62)


def _framing_body_size_greater_than_buffer() -> bytes:
    """Header declares 48 bytes of body but only 10 bytes follow."""
    return _pack_header(
        type_id="TRANSFORM", body_size_override=48,
    ) + b"\x00" * 10


def _framing_version_zero() -> bytes:
    """version=0 is not a valid OpenIGTLink protocol version."""
    body = _valid_transform_body()
    return _pack_header(version=0, body=body) + body


def _framing_version_future() -> bytes:
    """version=99 — a claimed-future version we don't recognize."""
    body = _valid_transform_body()
    return _pack_header(version=99, body=body) + body


def _framing_crc_mismatch() -> bytes:
    """Valid TRANSFORM body with deliberately wrong CRC."""
    body = _valid_transform_body()
    return _pack_header(body=body, crc_override=0xDEADBEEFCAFEF00D) + body


def _framing_ext_header_too_small() -> bytes:
    """v2 body claims ext_header_size=8, below the 12-byte minimum."""
    # Body = 12 bytes: 2 (eh=8!) + 2 (mh=0) + 4 (ms=0) + 4 (msg_id=0) = 12
    # We emit 12 bytes but claim ext_header_size=8.
    ext_header = struct.pack(">HHII", 8, 0, 0, 0)
    return _pack_header(version=2, body=ext_header) + ext_header


def _framing_ext_header_exceeds_body() -> bytes:
    """v2 body has ext_header_size=100 but body is only 12 bytes."""
    ext_header = struct.pack(">HHII", 100, 0, 0, 0)
    return _pack_header(version=2, body=ext_header) + ext_header


def _framing_metadata_region_overflow() -> bytes:
    """Extended header claims metadata_size=1000 but body has no metadata."""
    # metadata_header_size=4 + metadata_size=1000 > content region.
    ext_header = struct.pack(">HHII", 12, 4, 1000, 0)
    body = ext_header  # no content, no metadata
    return _pack_header(version=2, body=body) + body


def _framing_metadata_count_truncated() -> bytes:
    """Metadata count claims 5 entries but metadata_header_size only fits 1."""
    # metadata_header_size = 2 + 8*1 = 10 bytes. But count says 5, which
    # needs 2 + 8*5 = 42 bytes.
    content = b""
    metadata_header = struct.pack(">H", 5) + struct.pack(">HHI", 3, 3, 0)
    metadata_body = b""
    full_body = (
        struct.pack(">HHII", 12, len(metadata_header), len(metadata_body), 0)
        + content
        + metadata_header
        + metadata_body
    )
    return _pack_header(version=2, body=full_body) + full_body


def _framing_metadata_value_size_overflow() -> bytes:
    """Metadata value_size = 0xFFFFFFFF points way past buffer end."""
    content = b""
    metadata_header = struct.pack(">H", 1) + struct.pack(">HHI", 3, 3, 0xFFFFFFFF)
    metadata_body = b"key"  # truncated — we don't have 4G of value
    full_body = (
        struct.pack(">HHII", 12, len(metadata_header), len(metadata_body), 0)
        + content
        + metadata_header
        + metadata_body
    )
    return _pack_header(version=2, body=full_body) + full_body


def _content_transform_body_47() -> bytes:
    """TRANSFORM with 47-byte body (must be exactly 48)."""
    body = _valid_transform_body()[:47]
    return _pack_header(body=body) + body


def _content_transform_body_49() -> bytes:
    """TRANSFORM with 49-byte body (must be exactly 48)."""
    body = _valid_transform_body() + b"\x00"
    return _pack_header(body=body) + body


def _content_position_body_20() -> bytes:
    """POSITION body size MUST ∈ {12, 24, 28}. 20 is malformed."""
    body = b"\x00" * 20
    return _pack_header(type_id="POSITION", body=body) + body


def _content_position_body_32() -> bytes:
    """POSITION body size MUST ∈ {12, 24, 28}. 32 is malformed."""
    body = b"\x00" * 32
    return _pack_header(type_id="POSITION", body=body) + body


def _content_ndarray_dim_size_mismatch() -> bytes:
    """NDARRAY declares dim=3 but the size array only has 2 uint16 entries."""
    # Layout: scalar_type(u8), dim(u8), size[dim](u16), data[...]
    # dim=3, but we only provide 2 uint16s in size region.
    body = bytes([11, 3]) + struct.pack(">HH", 5, 4)  # missing the 3rd dim
    return _pack_header(type_id="NDARRAY", body=body) + body


def _content_ndarray_dim_huge() -> bytes:
    """NDARRAY dim=200 — size array alone would be 400 bytes but body is 2."""
    body = bytes([11, 200])  # dim=200, no size data
    return _pack_header(type_id="NDARRAY", body=body) + body


def _content_sensor_larray_mismatch() -> bytes:
    """SENSOR claims larray=10 floats but data region has 1 float's worth."""
    # larray(u8), status(u8), unit(u64), data[larray*8]
    body = bytes([10, 0]) + struct.pack(">Q", 0) + struct.pack(">d", 1.0)
    return _pack_header(type_id="SENSOR", body=body) + body


def _content_string_length_overflow() -> bytes:
    """STRING length prefix exceeds remaining body."""
    # STRING body: encoding(u16), length(u16), value[length]
    # We claim length=100 but value only has 5 bytes.
    body = struct.pack(">HH", 3, 100) + b"hello"
    return _pack_header(type_id="STRING", body=body) + body


def _content_bind_ncmessages_huge() -> bytes:
    """BIND ncmessages=10000 but body is only 2 bytes."""
    body = struct.pack(">H", 10000)
    return _pack_header(type_id="BIND", body=body) + body


# ---------------------------------------------------------------------------
# Corpus definition — the single source of truth
# ---------------------------------------------------------------------------


@dataclasses.dataclass
class _Spec:
    """Description of one negative-corpus entry, pre-build."""
    name: str
    path: str
    description: str
    error_class: str
    spec_reference: str
    builder: Builder
    known_issue: str = ""
    currently_accepted_by: tuple[str, ...] = ()


_CORPUS: list[_Spec] = [
    # --- Framing: outer header ---
    _Spec(
        name="framing_header_truncated_42",
        path="framing_header/truncated_42.bin",
        description="Only 42 bytes supplied; outer header needs 58.",
        error_class="SHORT_BUFFER",
        spec_reference="protocol/v3.md §Outer header",
        builder=_framing_truncated_header_42,
    ),
    _Spec(
        name="framing_header_truncated_57",
        path="framing_header/truncated_57.bin",
        description="57 bytes — one short of a complete header.",
        error_class="SHORT_BUFFER",
        spec_reference="protocol/v3.md §Outer header",
        builder=_framing_truncated_header_57,
    ),
    _Spec(
        name="framing_header_body_size_overflow",
        path="framing_header/body_size_overflow.bin",
        description="Header declares body_size=2^62; receiver has no such body.",
        error_class="SHORT_BUFFER",
        spec_reference="protocol/v3.md §Outer header",
        builder=_framing_body_size_overflow,
    ),
    _Spec(
        name="framing_header_body_size_greater_than_buffer",
        path="framing_header/body_size_greater_than_buffer.bin",
        description="Header declares 48-byte TRANSFORM body, only 10 bytes follow.",
        error_class="SHORT_BUFFER",
        spec_reference="protocol/v3.md §Outer header",
        builder=_framing_body_size_greater_than_buffer,
    ),
    _Spec(
        name="framing_header_version_zero",
        path="framing_header/version_zero.bin",
        description="Protocol version=0 is not defined.",
        error_class="MALFORMED",
        spec_reference="protocol/v3.md §Version field",
        builder=_framing_version_zero,
    ),
    _Spec(
        name="framing_header_version_future",
        path="framing_header/version_future.bin",
        description="Protocol version=99 is claimed-future; receiver does not recognize.",
        error_class="MALFORMED",
        spec_reference="protocol/v3.md §Version field",
        builder=_framing_version_future,
    ),
    _Spec(
        name="framing_header_crc_mismatch",
        path="framing_header/crc_mismatch.bin",
        description="Valid TRANSFORM body with deliberately wrong CRC-64.",
        error_class="CRC_MISMATCH",
        spec_reference="protocol/v3.md §Outer header (CRC)",
        builder=_framing_crc_mismatch,
    ),

    # --- Framing: v2/v3 extended header ---
    _Spec(
        name="framing_ext_header_too_small",
        path="framing_ext_header/ext_header_size_below_minimum.bin",
        description="v2 extended_header.ext_header_size=8, below the 12-byte minimum.",
        error_class="MALFORMED",
        spec_reference="protocol/v3.md §Extended header",
        builder=_framing_ext_header_too_small,
    ),
    _Spec(
        name="framing_ext_header_exceeds_body",
        path="framing_ext_header/ext_header_size_exceeds_body.bin",
        description="v2 ext_header_size=100 but body is only 12 bytes.",
        error_class="MALFORMED",
        spec_reference="protocol/v3.md §Extended header",
        builder=_framing_ext_header_exceeds_body,
    ),

    # --- Framing: metadata region ---
    _Spec(
        name="framing_metadata_region_overflow",
        path="framing_metadata/region_overflow.bin",
        description="metadata_size=1000 but body has no room for a metadata region.",
        error_class="MALFORMED",
        spec_reference="protocol/v3.md §Metadata layout",
        builder=_framing_metadata_region_overflow,
    ),
    _Spec(
        name="framing_metadata_count_truncated",
        path="framing_metadata/count_truncated.bin",
        description="metadata index claims 5 entries but only contains 1 entry's worth of bytes.",
        error_class="MALFORMED",
        spec_reference="protocol/v3.md §Metadata layout",
        builder=_framing_metadata_count_truncated,
    ),
    _Spec(
        name="framing_metadata_value_size_overflow",
        path="framing_metadata/value_size_overflow.bin",
        description="metadata entry's value_size=0xFFFFFFFF, far past buffer end.",
        error_class="MALFORMED",
        spec_reference="protocol/v3.md §Metadata layout",
        builder=_framing_metadata_value_size_overflow,
    ),

    # --- Content: TRANSFORM (fixed 48-byte body) ---
    _Spec(
        name="content_transform_body_47",
        path="content/transform_body_47.bin",
        description="TRANSFORM body must be exactly 48 bytes; 47 supplied.",
        error_class="MALFORMED",
        spec_reference="spec/schemas/transform.json",
        builder=_content_transform_body_47,
    ),
    _Spec(
        name="content_transform_body_49",
        path="content/transform_body_49.bin",
        description="TRANSFORM body must be exactly 48 bytes; 49 supplied.",
        error_class="MALFORMED",
        spec_reference="spec/schemas/transform.json",
        builder=_content_transform_body_49,
    ),

    # --- Content: POSITION (body_size ∈ {12, 24, 28}) ---
    _Spec(
        name="content_position_body_20",
        path="content/position_body_20.bin",
        description="POSITION body_size MUST ∈ {12, 24, 28}; 20 is invalid.",
        error_class="MALFORMED",
        spec_reference="spec/schemas/position.json legacy_notes",
        builder=_content_position_body_20,
    ),
    _Spec(
        name="content_position_body_32",
        path="content/position_body_32.bin",
        description="POSITION body_size MUST ∈ {12, 24, 28}; 32 is invalid.",
        error_class="MALFORMED",
        spec_reference="spec/schemas/position.json legacy_notes",
        builder=_content_position_body_32,
    ),

    # --- Content: NDARRAY (dim/size/data consistency) ---
    _Spec(
        name="content_ndarray_dim_size_mismatch",
        path="content/ndarray_dim_size_mismatch.bin",
        description="NDARRAY dim=3 but size array only has 2 entries.",
        error_class="MALFORMED",
        spec_reference="spec/schemas/ndarray.json",
        builder=_content_ndarray_dim_size_mismatch,
    ),
    _Spec(
        name="content_ndarray_dim_huge",
        path="content/ndarray_dim_huge.bin",
        description="NDARRAY dim=200; size array would need 400 bytes but body has 0.",
        error_class="MALFORMED",
        spec_reference="spec/schemas/ndarray.json",
        builder=_content_ndarray_dim_huge,
    ),

    # --- Content: SENSOR ---
    _Spec(
        name="content_sensor_larray_mismatch",
        path="content/sensor_larray_mismatch.bin",
        description="SENSOR larray=10 floats but data region has 1 float.",
        error_class="MALFORMED",
        spec_reference="spec/schemas/sensor.json",
        builder=_content_sensor_larray_mismatch,
    ),

    # --- Content: STRING ---
    _Spec(
        name="content_string_length_overflow",
        path="content/string_length_overflow.bin",
        description="STRING length=100 but value buffer has 5 bytes.",
        error_class="MALFORMED",
        spec_reference="spec/schemas/string.json",
        builder=_content_string_length_overflow,
    ),

    # --- Content: BIND ---
    _Spec(
        name="content_bind_ncmessages_huge",
        path="content/bind_ncmessages_huge.bin",
        description="BIND ncmessages=10000 but body has no room for entries.",
        error_class="MALFORMED",
        spec_reference="spec/schemas/bind.json",
        builder=_content_bind_ncmessages_huge,
    ),
]


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------


def build_corpus() -> list[NegativeEntry]:
    """Build every entry. Deterministic: same inputs → same bytes."""
    return [
        NegativeEntry(
            name=s.name,
            path=s.path,
            description=s.description,
            error_class=s.error_class,
            spec_reference=s.spec_reference,
            data=s.builder(),
            known_issue=s.known_issue,
            currently_accepted_by=s.currently_accepted_by,
        )
        for s in _CORPUS
    ]


def index_payload(entries: list[NegativeEntry]) -> dict:
    """Shape the entries into the JSON on-disk layout."""
    by_name = {}
    for e in entries:
        row: dict = {
            "path": e.path,
            "description": e.description,
            "error_class": e.error_class,
            "spec_reference": e.spec_reference,
            "size_bytes": len(e.data),
        }
        if e.known_issue:
            row["known_issue"] = e.known_issue
            row["currently_accepted_by"] = list(e.currently_accepted_by)
        by_name[e.name] = row
    return {
        "format_version": NEGATIVE_CORPUS_FORMAT_VERSION,
        "count": len(entries),
        "entries": by_name,
    }


def write_corpus(root: Path) -> list[Path]:
    """Write every entry's ``.bin`` + ``index.json`` under *root*.

    Returns the list of files written, sorted.
    """
    entries = build_corpus()
    written: list[Path] = []
    for e in entries:
        path = root / e.path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(e.data)
        written.append(path)
    idx_path = root / "index.json"
    idx_path.write_text(json.dumps(index_payload(entries), indent=2) + "\n")
    written.append(idx_path)
    return sorted(written)
