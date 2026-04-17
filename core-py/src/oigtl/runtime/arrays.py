"""Bulk primitive array runtime — numpy-native with array.array fallback.

Variable-count primitive fields (non-uint8) are returned from the
reference codec as raw big-endian wire bytes. This module promotes
them to a typed container for user-facing access:

- If the user installed the ``[numpy]`` extra, fields become
  ``np.ndarray`` with an explicit big-endian dtype
  (e.g. ``'>f4'`` for float32). Zero-copy from bytes via
  ``np.frombuffer``; endianness is tracked by the dtype and numpy
  handles it transparently for arithmetic.
- Without numpy, fields become ``array.array`` with the appropriate
  typecode, byteswapped to native order on little-endian hosts so
  ``arr[i]`` returns the correct scalar value.
- Fixed-count primitive fields stay as ``list[T]`` regardless — they
  are always small (≤12 elements in all 84 schemas) and the typed
  class declares them as ``list[T]`` directly.
- ``uint8`` (fixed or variable) stays as ``bytes`` — Python's native
  opaque-binary-data type, interoperates with hashlib/pickle/network
  libraries, and sidesteps any endianness question.

The pack direction accepts any of ``bytes``, ``ndarray``,
``array.array``, or ``list``/``tuple``; each is reduced to raw
big-endian bytes before being handed to the codec.
"""

from __future__ import annotations

import array
import os
import sys
from typing import Any

# Set OIGTL_NO_NUMPY=1 to force the stdlib `array.array` fallback
# even when numpy is installed. Useful for exercising the non-numpy
# code path in CI / differential fuzzing without maintaining a
# separate venv. Must be read at import time because `_HAS_NUMPY`
# is a module-level constant baked into call-site branches.
_FORCE_NO_NUMPY = os.environ.get("OIGTL_NO_NUMPY", "") == "1"

if _FORCE_NO_NUMPY:
    np = None  # type: ignore[assignment]
    _HAS_NUMPY = False
else:
    try:  # pragma: no cover - exercised by both branches in CI
        import numpy as np
        _HAS_NUMPY = True
    except ImportError:  # pragma: no cover
        np = None  # type: ignore[assignment]
        _HAS_NUMPY = False


# ---------------------------------------------------------------------------
# Type-code tables
# ---------------------------------------------------------------------------

# array.array typecodes. Deliberately avoids ``l``/``L`` (C long —
# platform-dependent size: 4 bytes on Windows, 8 bytes on 64-bit
# POSIX). ``i``/``q`` are guaranteed to be 4 and 8 bytes respectively
# since Python 3.3.
_ARRAY_CODE: dict[str, str] = {
    "int8":    "b", "uint8":   "B",
    "int16":   "h", "uint16":  "H",
    "int32":   "i", "uint32":  "I",
    "int64":   "q", "uint64":  "Q",
    "float32": "f", "float64": "d",
}

# Wire byte sizes — used for validation in pack path.
_WIRE_SIZE: dict[str, int] = {
    "int8": 1, "uint8": 1,
    "int16": 2, "uint16": 2,
    "int32": 4, "uint32": 4,
    "int64": 8, "uint64": 8,
    "float32": 4, "float64": 8,
}

# Big-endian numpy dtype strings. 1-byte types use the scalar dtype
# directly (endianness is irrelevant).
_NUMPY_BE_DTYPE: dict[str, str] = {
    "int8":    "|i1", "uint8":   "|u1",
    "int16":   ">i2", "uint16":  ">u2",
    "int32":   ">i4", "uint32":  ">u4",
    "int64":   ">i8", "uint64":  ">u8",
    "float32": ">f4", "float64": ">f8",
}


# Portability assertions — fail loudly on exotic platforms instead of
# producing silently-wrong wire bytes.
assert array.array("i").itemsize == 4, "expected 4-byte int32 typecode 'i'"
assert array.array("q").itemsize == 8, "expected 8-byte int64 typecode 'q'"
assert array.array("f").itemsize == 4, "expected 4-byte float32 typecode 'f'"
assert array.array("d").itemsize == 8, "expected 8-byte float64 typecode 'd'"


# ---------------------------------------------------------------------------
# Coercion (unpack direction)
# ---------------------------------------------------------------------------

def coerce_variable_array(value: Any, element_type: str) -> Any:
    """Coerce an input value into the preferred runtime container.

    Called from Pydantic ``@field_validator(mode='before')`` on
    variable-count primitive fields. Accepts anything sensible the
    user (or the codec) might pass:

    - ``bytes``: big-endian wire bytes straight from the codec.
    - ``np.ndarray``: used as-is when numpy is available; otherwise
      reduced to bytes → ``array.array``.
    - ``array.array``: used as-is if the typecode matches; reinterpreted
      via bytes if it doesn't.
    - ``list``/``tuple``: built into the preferred container.

    Returns ``np.ndarray`` if numpy is available, else ``array.array``.
    For ``uint8`` returns ``bytes`` unconditionally.
    """
    if element_type == "uint8":
        if isinstance(value, bytes):
            return value
        if isinstance(value, bytearray):
            return bytes(value)
        if _HAS_NUMPY and isinstance(value, np.ndarray):
            return value.astype(np.uint8).tobytes()
        if isinstance(value, array.array):
            return value.tobytes()
        return bytes(value)

    if element_type not in _ARRAY_CODE:
        raise ValueError(f"unknown primitive element type: {element_type!r}")

    if _HAS_NUMPY:
        dtype = _NUMPY_BE_DTYPE[element_type]
        if isinstance(value, np.ndarray):
            # Already an ndarray. If dtype mismatches the declared wire
            # type (e.g. user passed native-endian float32 for a
            # big-endian field), numpy handles the on-the-fly
            # reinterpretation during pack via astype — keep as-is.
            return value
        if isinstance(value, (bytes, bytearray)):
            # Zero-copy view over the codec output.
            return np.frombuffer(bytes(value), dtype=dtype)
        if isinstance(value, array.array):
            return np.frombuffer(value.tobytes(), dtype=dtype) \
                if _byteswap_needed(element_type) and \
                   value.typecode == _ARRAY_CODE[element_type] and \
                   sys.byteorder == "little" \
                else np.asarray(list(value), dtype=dtype)
        if isinstance(value, (list, tuple)):
            return np.asarray(value, dtype=dtype)
        raise TypeError(
            f"cannot coerce {type(value).__name__} to ndarray "
            f"(element_type={element_type!r})"
        )

    # numpy-free fallback: array.array
    code = _ARRAY_CODE[element_type]
    if isinstance(value, array.array):
        if value.typecode == code:
            return value
        # Wrong typecode — reinterpret via bytes. Assume the bytes are
        # already native-endian (array.array is always native-endian).
        arr = array.array(code)
        arr.frombytes(value.tobytes())
        return arr
    if isinstance(value, (bytes, bytearray)):
        arr = array.array(code)
        arr.frombytes(bytes(value))
        if _byteswap_needed(element_type) and sys.byteorder == "little":
            arr.byteswap()
        return arr
    if isinstance(value, (list, tuple)):
        return array.array(code, value)
    raise TypeError(
        f"cannot coerce {type(value).__name__} to array.array "
        f"(element_type={element_type!r})"
    )


def _byteswap_needed(element_type: str) -> bool:
    """True if element size > 1 byte (and thus endianness matters)."""
    return _WIRE_SIZE[element_type] > 1


# ---------------------------------------------------------------------------
# Serialization (pack direction)
# ---------------------------------------------------------------------------

def pack_variable_array(value: Any, element_type: str) -> bytes:
    """Reduce a runtime container to raw big-endian wire bytes.

    Called from generated ``pack()`` implementations before handing off
    to the reference codec. The codec's pack path already accepts
    ``bytes`` for primitive arrays (Phase 1 change), so this function
    normalizes everything to that shape.
    """
    if element_type == "uint8":
        if isinstance(value, bytes):
            return value
        if isinstance(value, bytearray):
            return bytes(value)
        if _HAS_NUMPY and isinstance(value, np.ndarray):
            return value.astype(np.uint8).tobytes()
        if isinstance(value, array.array):
            return value.tobytes()
        return bytes(value)

    if _HAS_NUMPY and isinstance(value, np.ndarray):
        # astype to the declared big-endian dtype — handles native→BE
        # conversion (and no-op when already big-endian).
        dtype = _NUMPY_BE_DTYPE[element_type]
        return value.astype(dtype).tobytes()

    if isinstance(value, array.array):
        if _byteswap_needed(element_type) and sys.byteorder == "little":
            # Swap into a copy so the caller's array stays native-endian.
            swapped = array.array(value.typecode)
            swapped.frombytes(value.tobytes())
            swapped.byteswap()
            return swapped.tobytes()
        return value.tobytes()

    if isinstance(value, (bytes, bytearray)):
        # Assume already wire-format big-endian.
        return bytes(value)

    if isinstance(value, (list, tuple)):
        # Let the codec's struct.pack path handle list → bytes.
        # Returning the list here would require the codec to support
        # it, which it does; but we roundtrip through bytes to keep
        # the generated call-site uniform.
        import struct
        from oigtl_corpus_tools.codec.primitives import FORMAT
        return struct.pack(">" + FORMAT[element_type] * len(value), *value)

    raise TypeError(
        f"cannot pack {type(value).__name__} as {element_type!r} array"
    )


# ---------------------------------------------------------------------------
# Empty-default factories (for Pydantic Field(default_factory=...))
# ---------------------------------------------------------------------------

def empty_variable_array(element_type: str) -> Any:
    """Return an empty container of the preferred runtime type."""
    if element_type == "uint8":
        return b""
    if _HAS_NUMPY:
        return np.empty(0, dtype=_NUMPY_BE_DTYPE[element_type])
    return array.array(_ARRAY_CODE[element_type])


__all__ = [
    "coerce_variable_array",
    "pack_variable_array",
    "empty_variable_array",
]
