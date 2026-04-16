"""Primitive type mappings — struct format characters and byte sizes.

All OpenIGTLink wire primitives are big-endian (network order).
"""

from __future__ import annotations

import struct

# Struct format character for each primitive type name.
# All used with '>' (big-endian) prefix.
FORMAT: dict[str, str] = {
    "uint8": "B",
    "uint16": "H",
    "uint32": "I",
    "uint64": "Q",
    "int8": "b",
    "int16": "h",
    "int32": "i",
    "int64": "q",
    "float32": "f",
    "float64": "d",
}

# Byte size of each primitive type.
SIZE: dict[str, int] = {name: struct.calcsize(fmt) for name, fmt in FORMAT.items()}


def pack_primitive(type_name: str, value: int | float) -> bytes:
    """Pack a single primitive value to big-endian bytes."""
    return struct.pack(">" + FORMAT[type_name], value)


def unpack_primitive(type_name: str, data: bytes, offset: int) -> tuple[int | float, int]:
    """Unpack a single primitive from *data* at *offset*.

    Returns ``(value, new_offset)``.
    """
    fmt = ">" + FORMAT[type_name]
    size = SIZE[type_name]
    (value,) = struct.unpack_from(fmt, data, offset)
    return value, offset + size
