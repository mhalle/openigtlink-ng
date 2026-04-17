"""Schema-driven validation policies shared across codec entry points.

These helpers capture spec-level constraints that are not expressible
through the field layout alone, so every code path that takes bytes off
the wire can enforce them uniformly. Keeping the checks here (rather
than inlined at each call site) prevents drift between ``unpack_message``
and the conformance oracle, and gives future code paths a single place
to pick them up.
"""

from __future__ import annotations

import struct
from functools import reduce
from operator import mul
from typing import Any


def check_body_size_set(schema: dict[str, Any], body_bytes_len: int) -> None:
    """Raise ValueError if schema declares body_size_set and body_bytes_len is not in it.

    No-op when the schema has no ``body_size_set`` field — the vast
    majority of message types accept any size compatible with their
    field layout. This is a focused guard for the handful of v1 legacy
    messages (POSITION, possibly others in future) whose spec explicitly
    whitelists sizes.

    The check runs before any field-level parsing so out-of-set bodies
    surface as a clean MALFORMED rejection rather than leaking through
    as a truncation of some interior field.
    """
    allowed = schema.get("body_size_set")
    if allowed is None:
        return
    if body_bytes_len not in allowed:
        type_id = schema.get("type_id", "<unknown>")
        raise ValueError(
            f"{type_id} body_size={body_bytes_len} is not in the "
            f"allowed set {sorted(allowed)}"
        )


# ---------------------------------------------------------------------------
# Named post-unpack invariants. Referenced by schemas via the
# ``post_unpack_invariant`` key. Each invariant validates cross-field
# constraints that cannot be expressed at field-level — most often
# "this variable-count primitive-array field's byte length must equal
# a function of some other fields' values".
#
# The vocabulary is closed: codecs in four languages re-implement each
# invariant by name, and the differential fuzzer holds them in sync.
# Adding a new invariant means touching corpus-tools (here), core-py
# (runtime/validators.py), core-cpp (runtime/validators.hpp), core-ts
# (runtime/validators.ts), and each codegen template's unpack emitter.
# ---------------------------------------------------------------------------


# Scalar type codes shared by IMAGE, SENSOR, NDARRAY, VIDEOMETA.
# Keys: wire-value. Values: bytes per scalar element.
#   2=int8, 3=uint8, 4=int16, 5=uint16, 6=int32, 7=uint32,
#   10=float32, 11=float64, 13=complex (two float64 per element)
_SCALAR_BYTES: dict[int, int] = {
    2: 1, 3: 1, 4: 2, 5: 2, 6: 4, 7: 4, 10: 4, 11: 8, 13: 16,
}

# IMAGE's scalar_type is a subset — complex (13) is NDARRAY-only.
_IMAGE_SCALAR_BYTES: dict[int, int] = {
    2: 1, 3: 1, 4: 2, 5: 2, 6: 4, 7: 4, 10: 4, 11: 8,
}


def _product(values: Any) -> int:
    """product() with identity 1, accepting any iterable of ints."""
    return reduce(mul, (int(v) for v in values), 1)


def _check_ndarray(msg: Any) -> None:
    """Enforce NDARRAY body invariants.

    1. scalar_type in {2,3,4,5,6,7,10,11,13}.
    2. len(data) == product(size) * bytes_per_scalar(scalar_type).

    Both are required by the spec text (spec/schemas/ndarray.json).
    Upstream igtl_ndarray_unpack does not enforce (2) — it carries an
    explicit TODO — which is why the differential fuzzer surfaced this
    as an upstream-rejects / we-accept divergence in reverse: upstream
    rejects malformed inputs the fuzzer generated, we accepted.
    """
    scalar_type = _field(msg, "scalar_type")
    if scalar_type not in _SCALAR_BYTES:
        raise ValueError(
            f"NDARRAY: invalid scalar_type={scalar_type}; "
            f"valid values are {sorted(_SCALAR_BYTES)}"
        )
    size = _as_uint_seq(_field(msg, "size"), elem_bytes=2)
    data = _field(msg, "data")
    expected = _product(size) * _SCALAR_BYTES[scalar_type]
    if len(data) != expected:
        raise ValueError(
            f"NDARRAY: data length {len(data)} does not match "
            f"product(size)={_product(size)} × "
            f"bytes_per_scalar({scalar_type})={_SCALAR_BYTES[scalar_type]} "
            f"= {expected}"
        )


def _check_image(msg: Any) -> None:
    """Enforce IMAGE body invariants.

    1. scalar_type in {2,3,4,5,6,7,10,11} (no complex).
    2. endian in {1,2,3}  (1=big, 2=little, 3=host).
    3. coord in {1,2}     (1=RAS, 2=LPS).
    4. subvol_offset[i] + subvol_size[i] <= size[i] for each axis i.
    5. len(pixels) == product(subvol_size) * num_components
                       * bytes_per_scalar(scalar_type).

    Invariant (5) is the one attackers exploit for heap OOB in
    upstream's legacy reader: dimensions are trusted without
    cross-checking against actual body_size - header_size.
    """
    scalar_type = _field(msg, "scalar_type")
    if scalar_type not in _IMAGE_SCALAR_BYTES:
        raise ValueError(
            f"IMAGE: invalid scalar_type={scalar_type}; "
            f"valid values are {sorted(_IMAGE_SCALAR_BYTES)}"
        )
    endian = _field(msg, "endian")
    if endian not in (1, 2, 3):
        raise ValueError(
            f"IMAGE: invalid endian={endian}; valid values are 1, 2, 3"
        )
    coord = _field(msg, "coord")
    if coord not in (1, 2):
        raise ValueError(
            f"IMAGE: invalid coord={coord}; valid values are 1 (RAS), 2 (LPS)"
        )
    # IMAGE's size / subvol_offset / subvol_size are fixed-count
    # (count=3) arrays, so our codec already returns them as list[int]
    # rather than raw bytes. Still route through _as_uint_seq for
    # symmetry + safety against future schema changes.
    size = _as_uint_seq(_field(msg, "size"), elem_bytes=2)
    subvol_offset = _as_uint_seq(_field(msg, "subvol_offset"), elem_bytes=2)
    subvol_size = _as_uint_seq(_field(msg, "subvol_size"), elem_bytes=2)
    for i, (off, sub, whole) in enumerate(zip(subvol_offset, subvol_size, size)):
        if int(off) + int(sub) > int(whole):
            raise ValueError(
                f"IMAGE: subvol_offset[{i}]+subvol_size[{i}]={off}+{sub} "
                f"exceeds size[{i}]={whole}"
            )
    num_components = _field(msg, "num_components")
    pixels = _field(msg, "pixels")
    expected = (
        _product(subvol_size)
        * int(num_components)
        * _IMAGE_SCALAR_BYTES[scalar_type]
    )
    if len(pixels) != expected:
        raise ValueError(
            f"IMAGE: pixels length {len(pixels)} does not match "
            f"product(subvol_size)={_product(subvol_size)} × "
            f"num_components={num_components} × "
            f"bytes_per_scalar({scalar_type})={_IMAGE_SCALAR_BYTES[scalar_type]} "
            f"= {expected}"
        )


def _field(msg: Any, name: str) -> Any:
    """Accessor that works on both dict (reference codec) and pydantic model."""
    if isinstance(msg, dict):
        return msg[name]
    return getattr(msg, name)


def _as_uint_seq(value: Any, *, elem_bytes: int) -> list[int]:
    """Normalize an array-ish field into a list of ints.

    The reference (dict) codec represents variable-count primitive
    arrays as raw big-endian wire bytes — e.g. NDARRAY.size shows up
    as ``bytes`` of length ``dim * 2`` rather than a list of uint16.
    The typed layers (numpy ndarray, ``array.array``) present a
    proper sequence of ints. This helper handles all three so
    invariants can loop over ints without caring which codec produced
    the input.
    """
    if isinstance(value, (bytes, bytearray, memoryview)):
        buf = bytes(value)
        if len(buf) % elem_bytes != 0:
            raise ValueError(
                f"array bytes length {len(buf)} not a multiple of "
                f"element size {elem_bytes}"
            )
        count = len(buf) // elem_bytes
        fmt_char = {1: "B", 2: "H", 4: "I", 8: "Q"}[elem_bytes]
        return list(struct.unpack(f">{count}{fmt_char}", buf))
    return [int(v) for v in value]


# Registry of named invariants. Keys match the schema's
# ``post_unpack_invariant`` string.
POST_UNPACK_INVARIANTS: dict[str, Any] = {
    "ndarray": _check_ndarray,
    "image": _check_image,
}


def run_post_unpack_invariant(schema: dict[str, Any], msg: Any) -> None:
    """Apply the schema-declared post-unpack invariant, if any."""
    name = schema.get("post_unpack_invariant")
    if name is None:
        return
    validator = POST_UNPACK_INVARIANTS.get(name)
    if validator is None:
        raise ValueError(
            f"schema references unknown post_unpack_invariant={name!r}; "
            f"known: {sorted(POST_UNPACK_INVARIANTS)}"
        )
    validator(msg)
