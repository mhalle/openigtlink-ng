"""Enumerations and constrained type aliases used across the meta-schema.

Kept in a standalone module so that every other schema module
(``element``, ``field``, ``message``) can import from here without
pulling in the full model hierarchy. No Pydantic models live here —
only ``str`` enums, typing aliases, and the primitive-size lookup
table.
"""

from __future__ import annotations

from enum import Enum
from typing import Annotated, Literal, Union

from pydantic import NonNegativeInt, StringConstraints


# ---------------------------------------------------------------------------
# Enumerations
# ---------------------------------------------------------------------------


class ProtocolVersion(str, Enum):
    """OpenIGTLink protocol version in which a feature first appeared."""

    V1 = "v1"
    V2 = "v2"
    V3 = "v3"
    V4 = "v4"


class PrimitiveType(str, Enum):
    """Fixed-width numeric types that can appear directly in a message body."""

    UINT8 = "uint8"
    UINT16 = "uint16"
    UINT32 = "uint32"
    UINT64 = "uint64"
    INT8 = "int8"
    INT16 = "int16"
    INT32 = "int32"
    INT64 = "int64"
    FLOAT32 = "float32"
    FLOAT64 = "float64"


class FieldType(str, Enum):
    """Composite field types — everything that isn't a numeric primitive.

    - ``fixed_string`` requires ``size_bytes``.
    - ``length_prefixed_string`` requires a ``length_prefix_type``.
    - ``trailing_string`` consumes all remaining body bytes; MUST be the
      last field in its message.
    - ``fixed_bytes`` requires ``size_bytes``.
    - ``array`` holds a variable number of elements of a single type;
      element count is given by ``count`` or ``count_from``.
    - ``struct_array`` — historical alias for ``array`` with a struct
      element; kept for symmetry with upstream terminology.
    - ``struct`` — composite element with a named ``fields`` list. Used
      as an array element to describe per-entry layouts like POINT's
      136-byte element or TDATA's 70-byte element.
    """

    FIXED_STRING = "fixed_string"
    LENGTH_PREFIXED_STRING = "length_prefixed_string"
    TRAILING_STRING = "trailing_string"
    FIXED_BYTES = "fixed_bytes"
    ARRAY = "array"
    STRUCT_ARRAY = "struct_array"
    STRUCT = "struct"


class Endianness(str, Enum):
    """Byte order for a multi-byte field on the wire."""

    BIG = "big"
    LITTLE = "little"


class Encoding(str, Enum):
    """Text encoding for string-typed fields."""

    ASCII = "ascii"
    UTF_8 = "utf-8"
    BINARY = "binary"


class CountSource(str, Enum):
    """How an array's element count is derived when not given explicitly.

    ``remaining`` means the count is computed at parse time as the number
    of elements that fit in the body bytes not consumed by earlier
    fields: ``count = (body_size - bytes_consumed_so_far) / element_size``.
    The division must be exact — a message whose body_size does not divide
    evenly is malformed. This matches the dominant pattern across the
    OpenIGTLink message catalog (CAPABILITY, POINT, TDATA, QTDATA,
    TRAJECTORY, IMGMETA, LBMETA), where the element count is implicit in
    the body size rather than carried in a header field.

    This enum is reserved for future growth (e.g. an NDARRAY-style
    product-of-sizes variant); today it has a single value.
    """

    REMAINING = "remaining"


# ---------------------------------------------------------------------------
# Shared constrained aliases
# ---------------------------------------------------------------------------


# Identifier matching the snake_case convention used for field names.
LowerIdentifier = Annotated[
    str,
    StringConstraints(pattern=r"^[a-z][a-z0-9_]*$"),
]

# Identifier matching the convention used for message type / struct names.
UpperIdentifier = Annotated[
    str,
    StringConstraints(pattern=r"^[A-Z][A-Z0-9_]*$"),
]

# Wire type identifier — ASCII, up to 12 bytes, all upper/digit/underscore.
TypeIdString = Annotated[
    str,
    StringConstraints(min_length=1, max_length=12, pattern=r"^[A-Z0-9_]+$"),
]

# Size or body_size: non-negative integer or the literal "variable".
Size = Union[NonNegativeInt, Literal["variable"]]

# Array length: either an inline integer count or a sibling-field name.
CountValue = Union[NonNegativeInt, LowerIdentifier]


# ---------------------------------------------------------------------------
# Primitive-size lookup
# ---------------------------------------------------------------------------


# Byte sizes of each primitive type. Used to statically derive element
# size for ``count_from: remaining`` validation.
PRIMITIVE_BYTE_SIZES: dict[PrimitiveType, int] = {
    PrimitiveType.UINT8: 1,
    PrimitiveType.UINT16: 2,
    PrimitiveType.UINT32: 4,
    PrimitiveType.UINT64: 8,
    PrimitiveType.INT8: 1,
    PrimitiveType.INT16: 2,
    PrimitiveType.INT32: 4,
    PrimitiveType.INT64: 8,
    PrimitiveType.FLOAT32: 4,
    PrimitiveType.FLOAT64: 8,
}
