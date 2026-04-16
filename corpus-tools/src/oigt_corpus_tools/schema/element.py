"""Inline element descriptor for array-typed fields.

An :class:`ElementDescriptor` describes the wire shape of a single
element inside an ``array`` / ``struct_array`` field — for example,
CAPABILITY's 12-byte fixed-string elements, or POINT's 136-byte struct
elements with named sub-fields.

The module intentionally does not import :class:`FieldSchema` directly:
``ElementDescriptor.fields`` is a forward reference (``"FieldSchema"``)
resolved when :mod:`oigt_corpus_tools.schema.field` finishes loading and
calls ``ElementDescriptor.model_rebuild()``. This layering keeps the
mutual recursion between element and field types expressible without
circular imports.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Optional, Union

from pydantic import BaseModel, ConfigDict, Field, model_validator

from oigt_corpus_tools.schema.types import (
    Encoding,
    Endianness,
    FieldType,
    PrimitiveType,
    Size,
    UpperIdentifier,
)

if TYPE_CHECKING:
    # Resolved at runtime by field.py's ``ElementDescriptor.model_rebuild()``.
    from oigt_corpus_tools.schema.field import FieldSchema  # noqa: F401


class ElementDescriptor(BaseModel):
    """Inline description of an array's element type.

    Used when an array / struct_array field has elements that are more
    complex than a single primitive but simpler than a full referenced
    struct — for example, CAPABILITY's array of 12-byte null-padded
    ASCII strings, where each element is a ``fixed_string`` with
    ``size_bytes: 12``.

    An ElementDescriptor carries only the *wire-shape* attributes
    (``type``, ``size_bytes``, ``encoding``, etc.) — not field-level
    attributes like ``name`` or top-level attributes like ``count``,
    since it describes what a single element looks like, not a field in
    its own right. Descriptions and notes are optional: an element is
    typically documented at its parent field's description.
    """

    model_config = ConfigDict(extra="forbid")

    type: Union[PrimitiveType, FieldType] = Field(
        description=(
            "Wire type of the element. Primitives are the common case; "
            "``fixed_string`` / ``fixed_bytes`` let an array carry "
            "uniformly-sized structured elements."
        ),
    )
    size_bytes: Optional[Size] = Field(
        default=None,
        description=(
            "Element byte size. Required when ``type`` is "
            "``fixed_string`` or ``fixed_bytes``."
        ),
    )
    encoding: Optional[Encoding] = Field(
        default=None,
        description="For string-typed elements: the expected text encoding.",
    )
    endianness: Optional[Endianness] = Field(
        default=None,
        description=(
            "Byte order. Defaults to big-endian (OpenIGTLink convention)."
        ),
    )
    null_padded: Optional[bool] = Field(
        default=None,
        description=(
            "For ``fixed_string`` elements: whether values shorter than "
            "``size_bytes`` are null-padded. Defaults to true."
        ),
    )
    length_prefix_type: Optional[PrimitiveType] = Field(
        default=None,
        description=(
            "For ``length_prefixed_string`` elements: the integer type "
            "of the length prefix."
        ),
    )
    description: Optional[str] = Field(
        default=None,
        description=(
            "Optional. Elements are normally documented via the parent "
            "field's description; use this only when per-element docs "
            "would add clarity."
        ),
    )
    fields: Optional[list["FieldSchema"]] = Field(
        default=None,
        description=(
            "For ``struct`` elements: ordered list of named sub-fields. "
            "Each sub-field is a full FieldSchema (primitive, array, or "
            "nested struct). Required when ``type`` is ``struct`` and "
            "MUST NOT be set for other types."
        ),
    )

    @model_validator(mode="after")
    def _check_type_requirements(self) -> ElementDescriptor:
        if self.type == FieldType.FIXED_STRING and self.size_bytes is None:
            raise ValueError(
                "element of type fixed_string requires size_bytes"
            )
        if self.type == FieldType.FIXED_BYTES and self.size_bytes is None:
            raise ValueError(
                "element of type fixed_bytes requires size_bytes"
            )
        if (
            self.type == FieldType.LENGTH_PREFIXED_STRING
            and self.length_prefix_type is None
        ):
            raise ValueError(
                "element of type length_prefixed_string requires "
                "length_prefix_type"
            )
        if self.type == FieldType.TRAILING_STRING:
            raise ValueError(
                "trailing_string cannot be an array element; it must be a "
                "top-level field and must be the last one in its message"
            )
        if self.type == FieldType.STRUCT:
            if not self.fields:
                raise ValueError(
                    "element of type struct requires a non-empty `fields` list"
                )
        elif self.fields is not None:
            raise ValueError(
                f"`fields` is only allowed on struct elements; "
                f"got type={self.type.value if isinstance(self.type, FieldType) else self.type}"
            )
        return self


# Array element type: a primitive, an inline descriptor, or the name of
# a struct type (struct references are not yet supported by codegen).
ElementType = Union[PrimitiveType, ElementDescriptor, UpperIdentifier]
