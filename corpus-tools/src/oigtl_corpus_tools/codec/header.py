"""58-byte OpenIGTLink message header — parse and emit.

The header is invariant across protocol versions. Its layout is
described by ``spec/schemas/framing_header.json`` but hard-coded here
for efficiency and because the header must be parsed *before* we know
which body schema to load.
"""

from __future__ import annotations

import struct
from typing import Any

from oigtl_corpus_tools.codec.crc64 import crc64

HEADER_SIZE = 58
_HEADER_FMT = ">H12s20sQQQ"  # version, type[12], device_name[20], timestamp, body_size, crc

assert struct.calcsize(_HEADER_FMT) == HEADER_SIZE


def unpack_header(data: bytes) -> dict[str, Any]:
    """Parse a 58-byte header from *data*.

    Returns a dict with keys: ``version``, ``type``, ``device_name``,
    ``timestamp``, ``body_size``, ``crc``.

    Raises :class:`ValueError` on short data.
    """
    if len(data) < HEADER_SIZE:
        raise ValueError(
            f"header requires {HEADER_SIZE} bytes, got {len(data)}"
        )
    version, type_raw, name_raw, timestamp, body_size, crc_val = struct.unpack_from(
        _HEADER_FMT, data, 0
    )
    return {
        "version": version,
        "type": type_raw.split(b"\x00", 1)[0].decode("ascii"),
        "device_name": name_raw.split(b"\x00", 1)[0].decode("ascii"),
        "timestamp": timestamp,
        "body_size": body_size,
        "crc": crc_val,
    }


def pack_header(
    *,
    version: int,
    type_id: str,
    device_name: str,
    timestamp: int,
    body: bytes,
) -> bytes:
    """Build a 58-byte header for the given *body*.

    CRC is computed automatically over *body*.
    """
    type_bytes = type_id.encode("ascii").ljust(12, b"\x00")[:12]
    name_bytes = device_name.encode("ascii").ljust(20, b"\x00")[:20]
    crc_val = crc64(body)
    return struct.pack(
        _HEADER_FMT,
        version,
        type_bytes,
        name_bytes,
        timestamp,
        len(body),
        crc_val,
    )


def verify_crc(header: dict[str, Any], body: bytes) -> None:
    """Raise :class:`ValueError` if the CRC in *header* does not match *body*."""
    computed = crc64(body)
    if computed != header["crc"]:
        raise ValueError(
            f"CRC mismatch: header declares 0x{header['crc']:016X}, "
            f"body computes 0x{computed:016X}"
        )
