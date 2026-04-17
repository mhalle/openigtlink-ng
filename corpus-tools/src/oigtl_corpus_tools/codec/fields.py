"""Schema-driven field pack/unpack — the heart of the reference codec.

Walks the ``fields`` list from a :class:`MessageSchema` sequentially,
using only the field's ``type``, ``size_bytes``, ``count``,
``count_from``, ``element_type``, ``encoding``, and
``length_prefix_type`` attributes. No per-message-type code.

Pack/unpack are symmetric: ``unpack_fields(schema, pack_fields(schema, values)) == values``
for any well-formed *values* dict (modulo floating-point round-trip).
"""

from __future__ import annotations

import struct
from typing import Any

from oigtl_corpus_tools.codec.primitives import FORMAT, SIZE, pack_primitive, unpack_primitive


# ---------------------------------------------------------------------------
# Element byte size (mirrors the schema validator's _try_element_byte_size)
# ---------------------------------------------------------------------------

def _element_byte_size(element_type: Any) -> int:
    """Return the byte size of one array element.

    Raises :class:`ValueError` if the element size cannot be determined
    statically (should not happen for validated schemas).
    """
    if isinstance(element_type, str):
        # Primitive type name (e.g. "uint8", "float32").
        if element_type in SIZE:
            return SIZE[element_type]
        raise ValueError(f"unknown primitive element type: {element_type!r}")
    if isinstance(element_type, dict):
        t = element_type.get("type")
        if t in SIZE:
            return SIZE[t]
        if t in ("fixed_string", "fixed_bytes"):
            return element_type["size_bytes"]
        if t == "struct":
            return sum(_field_byte_size(f) for f in element_type["fields"])
        raise ValueError(f"cannot determine element size for type={t!r}")
    raise ValueError(f"unexpected element_type: {element_type!r}")


def _field_byte_size(field: dict[str, Any]) -> int:
    """Return the statically-known byte size of a field."""
    t = field["type"]
    if t in SIZE:
        return SIZE[t]
    if t in ("fixed_string", "fixed_bytes"):
        return field["size_bytes"]
    if t == "array":
        elem_size = _element_byte_size(field["element_type"])
        count = field.get("count")
        if isinstance(count, int):
            return elem_size * count
        # Dynamic count — cannot compute statically.
        raise ValueError(f"field '{field['name']}' has dynamic size")
    raise ValueError(f"cannot compute size for type={t!r}")


# ---------------------------------------------------------------------------
# Unpack
# ---------------------------------------------------------------------------

def _unpack_element(element_type: Any, data: bytes, offset: int) -> tuple[Any, int]:
    """Unpack a single array element."""
    if isinstance(element_type, str):
        # Primitive.
        return unpack_primitive(element_type, data, offset)
    if isinstance(element_type, dict):
        t = element_type.get("type")
        if t in FORMAT:
            return unpack_primitive(t, data, offset)
        if t == "fixed_string":
            size = element_type["size_bytes"]
            raw = data[offset : offset + size]
            value = raw.split(b"\x00", 1)[0].decode(
                element_type.get("encoding", "ascii")
            )
            return value, offset + size
        if t == "fixed_bytes":
            size = element_type["size_bytes"]
            return bytes(data[offset : offset + size]), offset + size
        if t == "struct":
            result: dict[str, Any] = {}
            for sub_field in element_type["fields"]:
                val, offset = _unpack_field(sub_field, data, offset, result, len(data))
                result[sub_field["name"]] = val
            return result, offset
    raise ValueError(f"cannot unpack element_type={element_type!r}")


def _resolve_count(
    field: dict[str, Any],
    values: dict[str, Any],
    data: bytes,
    offset: int,
) -> int:
    """Determine the element count for an array field."""
    count = field.get("count")
    if count is not None:
        if isinstance(count, int):
            return count
        # Sibling field name.
        return int(values[count])

    count_from = field.get("count_from")
    if count_from == "remaining":
        elem_size = _element_byte_size(field["element_type"])
        remaining = len(data) - offset
        if elem_size == 0:
            return 0
        if remaining % elem_size != 0:
            raise ValueError(
                f"field '{field['name']}': remaining {remaining} bytes "
                f"not divisible by element size {elem_size}"
            )
        return remaining // elem_size

    raise ValueError(f"field '{field['name']}': no count or count_from")


def _unpack_field(
    field: dict[str, Any],
    data: bytes,
    offset: int,
    values: dict[str, Any],
    body_size: int,
) -> tuple[Any, int]:
    """Unpack one field, return ``(value, new_offset)``."""
    t = field["type"]

    # --- Primitives ---
    if t in FORMAT:
        return unpack_primitive(t, data, offset)

    # --- Fixed string ---
    if t == "fixed_string":
        size = field["size_bytes"]
        raw = data[offset : offset + size]
        enc = field.get("encoding", "ascii")
        null_padded = field.get("null_padded", True)
        if null_padded:
            value = raw.split(b"\x00", 1)[0].decode(enc)
        else:
            value = raw.decode(enc)
        return value, offset + size

    # --- Fixed bytes ---
    if t == "fixed_bytes":
        size = field["size_bytes"]
        return bytes(data[offset : offset + size]), offset + size

    # --- Length-prefixed string ---
    if t == "length_prefixed_string":
        prefix_type = field["length_prefix_type"]
        length, offset = unpack_primitive(prefix_type, data, offset)
        # Declared length must fit in the remaining buffer. Python's
        # `data[offset:offset + length]` slice is lenient (returns
        # fewer bytes when the end exceeds len(data)), which would
        # silently accept malformed inputs like "STRING length=100
        # with only 5 bytes of value". Reject explicitly.
        if offset + length > len(data):
            raise ValueError(
                f"field '{field['name']}': declared length {length} "
                f"exceeds remaining buffer ({len(data) - offset} bytes)"
            )
        enc = field.get("encoding", "ascii")
        raw = data[offset : offset + length]
        # 'binary' encoding = opaque bytes (STRING's value uses this
        # because the charset is declared in a sibling field).
        value = bytes(raw) if enc == "binary" else raw.decode(enc)
        return value, offset + length

    # --- Trailing string ---
    if t == "trailing_string":
        enc = field.get("encoding", "ascii")
        raw = data[offset:]
        null_terminated = field.get("null_terminated", False)
        if null_terminated and raw and raw[-1:] == b"\x00":
            raw = raw[:-1]
        value = bytes(raw) if enc == "binary" else raw.decode(enc)
        return value, len(data)

    # --- Array / struct_array ---
    if t in ("array", "struct_array"):
        count = _resolve_count(field, values, data, offset)
        element_type = field["element_type"]

        # Fast path: primitive element type → single slice or one
        # struct.unpack_from call instead of a Python loop. Critical
        # for IMAGE / POLYDATA / NDARRAY where arrays are megabytes of
        # uniform primitive data.
        #
        # Variable-count arrays return raw wire bytes (big-endian); the
        # typed Python layer coerces to ndarray/array.array via
        # oigtl.runtime.arrays.coerce_variable_array. Fixed-count arrays
        # (count is a schema-literal int) stay as list[T] — they're
        # always small (≤12 elements across all 84 schemas) and the
        # typed field type stays `list[T]` for clean Pydantic validation.
        if isinstance(element_type, str) and element_type in FORMAT:
            is_fixed = isinstance(field.get("count"), int)
            elem_size = SIZE[element_type]
            total = elem_size * count
            # Bounds check: primitive-element array reads used to slice
            # `data[offset:offset+total]` without verifying the slice
            # fits in the buffer. Python's slice truncation silently
            # accepted short bodies (e.g. NDARRAY dim=3 with only 4 bytes
            # of size[], SENSOR larray=10 with 8 bytes of data). Reject
            # explicitly so short / tampered bodies raise instead of
            # decoding a truncated array.
            remaining = len(data) - offset
            if total > remaining:
                raise ValueError(
                    f"field {field['name']!r}: {count} × {elem_size} "
                    f"bytes exceeds remaining {remaining}"
                )
            if not is_fixed:
                return bytes(data[offset : offset + total]), offset + total
            if element_type == "uint8":
                # Fixed-count uint8 (rare — e.g. POINT.rgba with count=4).
                # Keep bytes representation for consistency with variable
                # uint8; Pydantic sees `bytes` either way.
                return bytes(data[offset : offset + total]), offset + total
            fmt = ">" + FORMAT[element_type] * count
            elements = list(struct.unpack_from(fmt, data, offset))
            return elements, offset + total

        elements = []
        for _ in range(count):
            elem, offset = _unpack_element(element_type, data, offset)
            elements.append(elem)
        return elements, offset

    raise ValueError(f"unsupported field type: {t!r}")


def unpack_fields(fields: list[dict[str, Any]], data: bytes) -> dict[str, Any]:
    """Unpack all *fields* from *data*, returning a name→value dict."""
    values: dict[str, Any] = {}
    offset = 0
    for field in fields:
        val, offset = _unpack_field(field, data, offset, values, len(data))
        values[field["name"]] = val
    return values


# ---------------------------------------------------------------------------
# Pack
# ---------------------------------------------------------------------------

def _pack_element(element_type: Any, value: Any) -> bytes:
    """Pack a single array element to bytes."""
    if isinstance(element_type, str):
        return pack_primitive(element_type, value)
    if isinstance(element_type, dict):
        t = element_type.get("type")
        if t in FORMAT:
            return pack_primitive(t, value)
        if t == "fixed_string":
            size = element_type["size_bytes"]
            enc = element_type.get("encoding", "ascii")
            raw = value.encode(enc)
            return raw.ljust(size, b"\x00")[:size]
        if t == "fixed_bytes":
            size = element_type["size_bytes"]
            return bytes(value).ljust(size, b"\x00")[:size]
        if t == "struct":
            parts: list[bytes] = []
            for sub_field in element_type["fields"]:
                parts.append(_pack_field(sub_field, value.get(sub_field["name"])))
            return b"".join(parts)
    raise ValueError(f"cannot pack element_type={element_type!r}")


def _pack_field(field: dict[str, Any], value: Any) -> bytes:
    """Pack one field to bytes."""
    t = field["type"]

    if t in FORMAT:
        return pack_primitive(t, value)

    if t == "fixed_string":
        size = field["size_bytes"]
        enc = field.get("encoding", "ascii")
        raw = value.encode(enc) if isinstance(value, str) else value
        return raw.ljust(size, b"\x00")[:size]

    if t == "fixed_bytes":
        size = field["size_bytes"]
        return bytes(value).ljust(size, b"\x00")[:size]

    if t == "length_prefixed_string":
        prefix_type = field["length_prefix_type"]
        enc = field.get("encoding", "ascii")
        raw = value.encode(enc) if isinstance(value, str) else value
        return pack_primitive(prefix_type, len(raw)) + raw

    if t == "trailing_string":
        enc = field.get("encoding", "ascii")
        raw = value.encode(enc) if isinstance(value, str) else value
        if field.get("null_terminated", False):
            raw = raw + b"\x00"
        return raw

    if t in ("array", "struct_array"):
        element_type = field["element_type"]

        # Fast path: primitive element type → single slice or one
        # struct.pack call instead of a Python loop + concat. Hot for
        # IMAGE / POLYDATA / NDARRAY where arrays are megabytes of
        # primitive data.
        #
        # Accepts multiple input shapes so the typed layer can hand off
        # whatever representation is natural:
        #   - bytes/bytearray/memoryview: already wire-format big-endian
        #     (e.g. directly from unpack, or np.ndarray.tobytes() with a
        #     big-endian dtype). Pass through unchanged.
        #   - list/tuple of ints/floats: pack element-by-element via
        #     struct (the original path).
        # ndarray / array.array inputs are expected to arrive as bytes
        # from the typed layer's pack helper.
        if isinstance(element_type, str) and element_type in FORMAT:
            if isinstance(value, (bytes, bytearray, memoryview)):
                elem_size = SIZE[element_type]
                raw = bytes(value)
                if len(raw) % elem_size != 0:
                    raise ValueError(
                        f"bytes length {len(raw)} is not a multiple of "
                        f"{element_type} element size {elem_size}"
                    )
                return raw
            if element_type == "uint8":
                return bytes(value)
            fmt = ">" + FORMAT[element_type] * len(value)
            return struct.pack(fmt, *value)

        parts = [_pack_element(element_type, elem) for elem in value]
        return b"".join(parts)

    raise ValueError(f"unsupported field type: {t!r}")


def pack_fields(fields: list[dict[str, Any]], values: dict[str, Any]) -> bytes:
    """Pack all *fields* from *values* dict to bytes."""
    parts: list[bytes] = []
    for field in fields:
        name = field["name"]
        if name not in values:
            raise ValueError(f"missing value for field '{name}'")
        parts.append(_pack_field(field, values[name]))
    return b"".join(parts)
