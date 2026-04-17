"""Semantic-layer helpers for message types where wire element type
differs from the scientifically-relevant element type.

IMAGE and NDARRAY both carry their payload as ``uint8`` bytes on the
wire, with a sibling ``scalar_type`` code declaring how those bytes
should be interpreted (int8, int16, float32, etc.) and a ``size``
tuple declaring the array shape. The wire representation is correct
and round-trips byte-for-byte; these helpers provide the one-liner
users almost always want:

    >>> from oigtl.messages import Image, parse_message
    >>> from oigtl.semantic import pixel_array
    >>> msg = parse_message(wire_bytes)
    >>> arr = pixel_array(msg)            # np.ndarray with correct dtype + shape
    >>> arr.mean()
    123.4

These helpers require numpy. Without it there is no reasonable
return type for a multi-dimensional typed array, so they raise
``ImportError``. The base typed fields (``img.pixels``,
``ndarray.data``) remain accessible as ``bytes`` either way —
these helpers are layered on top.

Matches (roughly) pyigtl's ``ImageMessage.image`` property
behaviour, with one deliberate difference: we honor the IMAGE
``endian`` field per the v3 spec, whereas pyigtl ignores it.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:  # pragma: no cover
    import numpy as np
    from oigtl.messages import Image, Ndarray


# ---------------------------------------------------------------------------
# scalar_type → numpy dtype mapping (shared by IMAGE and NDARRAY)
# ---------------------------------------------------------------------------

# Values from the v3 spec §igtl_util.h — used by IMAGE, NDARRAY, and
# (for historical compat) a handful of other message types.
#   2=int8, 3=uint8, 4=int16, 5=uint16, 6=int32, 7=uint32,
#   10=float32, 11=float64, 13=complex (float32 pair)
_SCALAR_NUMPY_CODE: dict[int, str] = {
    2:  "i1",
    3:  "u1",
    4:  "i2",
    5:  "u2",
    6:  "i4",
    7:  "u4",
    10: "f4",
    11: "f8",
    13: "c8",   # complex64
}


def _require_numpy():
    try:
        import numpy as np  # noqa: F401
        return np
    except ImportError as e:  # pragma: no cover - exercised by users
        raise ImportError(
            "oigtl.semantic helpers require numpy. Install with "
            "'pip install oigtl[numpy]' (or 'uv add oigtl --extra numpy')."
        ) from e


def _dtype_for(scalar_type: int, *, big_endian: bool) -> str:
    """Return the numpy dtype string for a given scalar_type code."""
    if scalar_type not in _SCALAR_NUMPY_CODE:
        raise ValueError(
            f"unknown scalar_type {scalar_type}; "
            f"expected one of {sorted(_SCALAR_NUMPY_CODE)}"
        )
    code = _SCALAR_NUMPY_CODE[scalar_type]
    if code in ("i1", "u1"):
        # 1-byte scalars: endianness is irrelevant.
        return code
    return (">" if big_endian else "<") + code


# ---------------------------------------------------------------------------
# IMAGE
# ---------------------------------------------------------------------------


def pixel_array(image: "Image") -> "np.ndarray":
    """Return ``image.pixels`` reshaped + dtyped per the image's header.

    Shape is ``(depth, height, width)`` for single-component images
    (numpy convention: outermost axis first) — this matches PLUS
    toolkit and 3D Slicer conventions. For multi-component images
    (``num_components > 1``), an extra trailing axis of that size
    is appended, so shape becomes ``(depth, height, width, C)``.

    Endianness honours the IMAGE ``endian`` field:
      endian=1 → big-endian bytes
      endian=2 → little-endian bytes
      endian=3 → host (interpreted per ``sys.byteorder``)

    This is a zero-copy view over the underlying bytes. Modifying
    the returned array modifies the message's pixel buffer (unless
    the dtype's endianness differs from the host, in which case
    numpy ops may silently copy).
    """
    np = _require_numpy()

    # Endian resolution. v3 spec §IMAGE body:
    #   1 = BIG,  2 = LITTLE,  3 = HOST (sender's byte order = receiver's)
    endian = int(getattr(image, "endian", 1))
    if endian == 1:
        big_endian = True
    elif endian == 2:
        big_endian = False
    elif endian == 3:
        import sys
        big_endian = sys.byteorder == "big"
    else:
        raise ValueError(f"IMAGE.endian: expected 1/2/3, got {endian}")

    dtype = _dtype_for(int(image.scalar_type), big_endian=big_endian)
    arr = np.frombuffer(image.pixels, dtype=dtype)

    # IMAGE.size is [i, j, k] on the wire. Numpy uses outermost-axis-
    # first indexing, so reverse for the image volume axis order.
    sx, sy, sz = (int(image.size[0]), int(image.size[1]), int(image.size[2]))
    n_comp = int(image.num_components)
    if n_comp == 1:
        return arr.reshape((sz, sy, sx))
    return arr.reshape((sz, sy, sx, n_comp))


# ---------------------------------------------------------------------------
# NDARRAY
# ---------------------------------------------------------------------------


def data_array(ndarray_msg: "Ndarray") -> "np.ndarray":
    """Return NDARRAY ``data`` reshaped + dtyped per ``scalar_type`` and ``size``.

    NDARRAY.size is a variable-count uint16 vector of length ``dim``;
    after Phase 1 it arrives as raw big-endian wire bytes in
    ``msg.size``. We decode it here to produce the shape tuple.

    Shape follows wire order (i.e., not reversed). This matches the
    upstream ``igtl::NDArrayMessage::GetArray`` convention.

    Endianness: NDARRAY does not carry an endian field; payload is
    always big-endian per spec.
    """
    np = _require_numpy()

    # size is variable-count uint16. In the current typed layer it
    # arrives already as ndarray (with numpy) or array.array (stdlib
    # fallback), both supporting int-like indexing via list().
    size_tuple = tuple(int(x) for x in ndarray_msg.size)
    if len(size_tuple) != int(ndarray_msg.dim):
        raise ValueError(
            f"NDARRAY.size length {len(size_tuple)} != dim {ndarray_msg.dim}"
        )

    dtype = _dtype_for(int(ndarray_msg.scalar_type), big_endian=True)
    arr = np.frombuffer(ndarray_msg.data, dtype=dtype)
    return arr.reshape(size_tuple)


__all__ = ["pixel_array", "data_array"]
