"""Message-level pack/unpack — loads schemas, dispatches to field walker.

The public API for the reference codec. Usage::

    from oigtl_corpus_tools.codec import unpack_message, pack_message

    header, body = unpack_message(wire_bytes)
    wire_bytes = pack_message("TRANSFORM", "DeviceName", body_values)
"""

from __future__ import annotations

import json
from functools import lru_cache
from pathlib import Path
from typing import Any

from oigtl_corpus_tools.codec.crc64 import crc64
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields
from oigtl_corpus_tools.codec.header import (
    HEADER_SIZE,
    pack_header,
    unpack_header,
    verify_crc,
)
from oigtl_corpus_tools.paths import find_repo_root, schemas_dir


# ---------------------------------------------------------------------------
# Schema loading
# ---------------------------------------------------------------------------


def _build_type_id_index() -> dict[str, Path]:
    """Scan spec/schemas/ and build a type_id → file path index."""
    index: dict[str, Path] = {}
    sdir = schemas_dir(find_repo_root())
    for path in sorted(sdir.glob("*.json")):
        try:
            with open(path) as f:
                obj = json.load(f)
            tid = obj.get("type_id")
            if tid:
                index[tid] = path
        except (json.JSONDecodeError, KeyError):
            continue
    return index


@lru_cache(maxsize=1)
def _type_id_index() -> dict[str, Path]:
    return _build_type_id_index()


@lru_cache(maxsize=128)
def load_schema(type_id: str) -> dict[str, Any]:
    """Load and return the schema dict for a wire *type_id*.

    Raises :class:`KeyError` if no schema exists for the type_id.
    """
    index = _type_id_index()
    if type_id not in index:
        raise KeyError(f"no schema found for type_id={type_id!r}")
    with open(index[type_id]) as f:
        return json.load(f)


# ---------------------------------------------------------------------------
# Body pack/unpack
# ---------------------------------------------------------------------------


def unpack_body(schema: dict[str, Any], data: bytes) -> dict[str, Any]:
    """Unpack a message body using the given *schema*.

    *data* is the raw body bytes (after the 58-byte header, and after
    stripping the extended header / metadata in v3 if applicable).

    Returns a dict of field name → value.
    """
    fields = schema.get("fields", [])
    if not fields:
        return {}
    return unpack_fields(fields, data)


def pack_body(schema: dict[str, Any], values: dict[str, Any]) -> bytes:
    """Pack a message body from *values* using the given *schema*.

    Returns raw body bytes.
    """
    fields = schema.get("fields", [])
    if not fields:
        return b""
    return pack_fields(fields, values)


# ---------------------------------------------------------------------------
# Full message pack/unpack
# ---------------------------------------------------------------------------


def unpack_message(
    data: bytes,
    *,
    verify: bool = True,
) -> tuple[dict[str, Any], dict[str, Any]]:
    """Parse a complete wire message (header + body).

    Returns ``(header_dict, body_dict)``.

    If *verify* is True (default), CRC is checked and a
    :class:`ValueError` is raised on mismatch.

    Raises :class:`ValueError` on malformed data.
    Raises :class:`KeyError` if the type_id has no schema.
    """
    if len(data) < HEADER_SIZE:
        raise ValueError(
            f"message too short: {len(data)} bytes, need at least {HEADER_SIZE}"
        )
    header = unpack_header(data)
    body_start = HEADER_SIZE
    body_end = HEADER_SIZE + header["body_size"]
    if len(data) < body_end:
        raise ValueError(
            f"message truncated: header declares body_size={header['body_size']}, "
            f"but only {len(data) - HEADER_SIZE} bytes available"
        )
    body_bytes = data[body_start:body_end]

    if verify:
        verify_crc(header, body_bytes)

    schema = load_schema(header["type"])
    body_values = unpack_body(schema, body_bytes)
    return header, body_values


def pack_message(
    type_id: str,
    device_name: str = "",
    values: dict[str, Any] | None = None,
    *,
    version: int = 1,
    timestamp: int = 0,
) -> bytes:
    """Build a complete wire message (header + body).

    Loads the schema for *type_id*, packs the body from *values*,
    computes CRC, and returns the full wire bytes.
    """
    schema = load_schema(type_id)
    body = pack_body(schema, values or {})
    header = pack_header(
        version=version,
        type_id=type_id,
        device_name=device_name,
        timestamp=timestamp,
        body=body,
    )
    return header + body
