"""Top-level field definition inside a message body.

:class:`FieldSchema` is the richest class in the meta-schema: it covers
primitives, strings (fixed / length-prefixed / trailing), fixed byte
buffers, and arrays with either a static count, a sibling-field count,
or a body-derived count.

The two private helpers ``_try_element_byte_size`` and
``_try_field_byte_size`` live here because they walk both
:class:`~.element.ElementDescriptor` and :class:`FieldSchema`
recursively. Keeping them co-located with :class:`FieldSchema` means the
``count_from=remaining`` validator can call them directly.

At module load the last statement is
``ElementDescriptor.model_rebuild(...)``, which resolves the
``"FieldSchema"`` forward reference inside
:attr:`ElementDescriptor.fields`. This is what enables struct elements
whose sub-fields are themselves arrays, fixed strings, or nested structs.
"""

from __future__ import annotations

from typing import Any, Optional, Union

from pydantic import BaseModel, ConfigDict, Field, model_validator

from oigt_corpus_tools.schema.element import ElementDescriptor, ElementType
from oigt_corpus_tools.schema.types import (
    CountSource,
    CountValue,
    Encoding,
    Endianness,
    FieldType,
    LowerIdentifier,
    PRIMITIVE_BYTE_SIZES,
    PrimitiveType,
    ProtocolVersion,
    Size,
)


# ---------------------------------------------------------------------------
# Static byte-size helpers
# ---------------------------------------------------------------------------


def _try_element_byte_size(element_type: Any) -> Optional[int]:
    """Return the element's statically-known byte size, or ``None``.

    ``None`` means the size cannot be derived without reading the data
    (e.g. the element is a struct reference whose layout is defined
    elsewhere by name, or a variable-length element type). Callers that
    need a known size to validate — notably ``count_from: remaining`` —
    use this helper and reject when ``None``.
    """
    if isinstance(element_type, PrimitiveType):
        return PRIMITIVE_BYTE_SIZES[element_type]
    if isinstance(element_type, ElementDescriptor):
        # Primitive element via descriptor: look up size by type.
        if isinstance(element_type.type, PrimitiveType):
            return PRIMITIVE_BYTE_SIZES[element_type.type]
        # Fixed-size composite element: size_bytes carries it (and the
        # ElementDescriptor validator has already guaranteed non-None
        # when `type` is fixed_string / fixed_bytes).
        if (
            element_type.type in (FieldType.FIXED_STRING, FieldType.FIXED_BYTES)
            and isinstance(element_type.size_bytes, int)
        ):
            return element_type.size_bytes
        # Struct element: sum the byte sizes of each named sub-field.
        # Any sub-field with a variable size poisons the sum to None.
        if element_type.type == FieldType.STRUCT and element_type.fields:
            total = 0
            for sub_field in element_type.fields:
                size = _try_field_byte_size(sub_field)
                if size is None:
                    return None
                total += size
            return total
        return None
    # UpperIdentifier (named struct) — layout lives elsewhere, not yet
    # resolvable at schema-validation time.
    return None


def _try_field_byte_size(field: "FieldSchema") -> Optional[int]:
    """Return the field's statically-known byte size, or ``None``.

    Mirrors :func:`_try_element_byte_size` but operates on a
    :class:`FieldSchema`, which has a richer set of attributes including
    ``count`` and ``count_from``. Used recursively to compute the size
    of a struct element by summing its sub-fields.
    """
    # Primitive field.
    if isinstance(field.type, PrimitiveType):
        return PRIMITIVE_BYTE_SIZES[field.type]
    # Fixed-size byte-strings / byte-buffers.
    if field.type in (FieldType.FIXED_STRING, FieldType.FIXED_BYTES):
        if isinstance(field.size_bytes, int):
            return field.size_bytes
        return None
    # Array / struct_array — compute as (element_size * count) when
    # both factors are known statically.
    if field.type in (FieldType.ARRAY, FieldType.STRUCT_ARRAY):
        element_size = _try_element_byte_size(field.element_type)
        if element_size is None:
            return None
        if isinstance(field.count, int):
            return element_size * field.count
        # count from a sibling field or count_from: dynamic, not
        # computable at schema-validation time.
        return None
    # Top-level struct field with inline fields list — unusual shape;
    # its size is the sum of its sub-fields' sizes. Not commonly used
    # because struct-shaped data is more idiomatically expressed as
    # a struct *element* inside an array of length 1.
    # Deferred until a real use case emerges.
    if field.type == FieldType.STRUCT:
        return None
    # length_prefixed_string, trailing_string: variable size.
    return None


# ---------------------------------------------------------------------------
# Field
# ---------------------------------------------------------------------------


class FieldSchema(BaseModel):
    """One field in a message body."""

    model_config = ConfigDict(extra="forbid")

    name: LowerIdentifier = Field(
        description=(
            "Field name, snake_case. Used as-is in codegen; language ports "
            "may apply idiomatic casing conversions."
        ),
    )
    type: Union[PrimitiveType, FieldType] = Field(
        description=(
            "Field type. Primitives are encoded in the order declared. "
            "Composite types require additional attributes "
            "(``size_bytes``, ``count``, ``element_type``, "
            "``length_prefix_type``, ``encoding``, etc.). "
            "``trailing_string`` consumes all remaining body bytes and "
            "MUST be the last field in its message."
        ),
    )
    description: str = Field(
        min_length=1,
        description="Concise description of the field's meaning. Required.",
    )
    rationale: Optional[str] = Field(
        default=None,
        description="Optional. Why this field is shaped this way.",
    )
    introduced_in: Optional[ProtocolVersion] = Field(
        default=None,
        description=(
            "Protocol version in which this field first appeared, if "
            "different from the message's ``introduced_in``."
        ),
    )
    endianness: Optional[Endianness] = Field(
        default=None,
        description=(
            "Byte order. Defaults to big-endian (OpenIGTLink convention). "
            "Rarely overridden; if overridden, explain in ``rationale``."
        ),
    )
    size_bytes: Optional[Size] = Field(
        default=None,
        description=(
            "Size in bytes on the wire. Required for ``fixed_string`` and "
            "``fixed_bytes``; optional for other types if derivable from "
            "``type`` alone."
        ),
    )
    count: Optional[CountValue] = Field(
        default=None,
        description=(
            "For ``array`` / ``struct_array``: either a fixed integer "
            "count, or the name of a sibling field that carries the count. "
            "Mutually exclusive with ``count_from``."
        ),
    )
    count_from: Optional[CountSource] = Field(
        default=None,
        description=(
            "For ``array`` / ``struct_array``: derive the element count "
            "implicitly rather than from a fixed integer or sibling field. "
            "Value ``remaining`` means ``count = (body_size - "
            "bytes_consumed_so_far) / element_size``, and the division "
            "must be exact. Mutually exclusive with ``count``; exactly one "
            "of the two is required on array / struct_array fields."
        ),
    )
    element_type: Optional[ElementType] = Field(
        default=None,
        description=(
            "For ``array``: the element's primitive type. For "
            "``struct_array``: the name of an inline struct (not yet "
            "supported)."
        ),
    )
    layout: Optional[str] = Field(
        default=None,
        description=(
            "Optional semantic-layout hint (e.g. 'column_major_3x4' for "
            "TRANSFORM's matrix). Codegen may use this for helper methods; "
            "it has no effect on wire encoding."
        ),
    )
    length_prefix_type: Optional[PrimitiveType] = Field(
        default=None,
        description=(
            "For ``length_prefixed_string``: the integer type used to "
            "encode the length prefix."
        ),
    )
    encoding: Optional[Encoding] = Field(
        default=None,
        description="For string types: the expected text encoding.",
    )
    null_padded: Optional[bool] = Field(
        default=None,
        description=(
            "For ``fixed_string``: whether values shorter than "
            "``size_bytes`` are padded with null bytes (true) or the full "
            "region is treated as the string (false). Defaults to true."
        ),
    )
    null_terminated: Optional[bool] = Field(
        default=None,
        description=(
            "For ``trailing_string``: whether the final byte on the wire "
            "MUST be 0x00. When true, the null is part of the field bytes "
            "(a non-empty string occupies ``len+1`` bytes) and the string "
            "value is the bytes excluding the final null. Defaults to "
            "false."
        ),
    )
    legacy_notes: Optional[list[str]] = Field(
        default=None,
        description=(
            "Historical quirks for this field that must be preserved for "
            "wire compatibility."
        ),
    )
    notes: Optional[list[str]] = Field(
        default=None,
        description=(
            "Other prose notes that do not fit ``rationale`` or "
            "``legacy_notes``."
        ),
    )

    @model_validator(mode="after")
    def _check_type_requirements(self) -> FieldSchema:
        if self.type == FieldType.FIXED_STRING and self.size_bytes is None:
            raise ValueError(
                f"field '{self.name}' of type fixed_string requires "
                f"size_bytes"
            )
        if (
            self.type == FieldType.LENGTH_PREFIXED_STRING
            and self.length_prefix_type is None
        ):
            raise ValueError(
                f"field '{self.name}' of type length_prefixed_string "
                f"requires length_prefix_type"
            )
        if self.type in (FieldType.ARRAY, FieldType.STRUCT_ARRAY):
            has_count = self.count is not None
            has_count_from = self.count_from is not None
            if has_count and has_count_from:
                raise ValueError(
                    f"field '{self.name}' of type {self.type.value} "
                    f"has both `count` and `count_from` set; exactly "
                    f"one is required"
                )
            if not has_count and not has_count_from:
                raise ValueError(
                    f"field '{self.name}' of type {self.type.value} "
                    f"requires one of `count` or `count_from`"
                )
            # count_from=remaining derives the element count from
            # body_size / element_size, so the element size must be
            # statically known at schema-validation time.
            if self.count_from == CountSource.REMAINING:
                if self.element_type is None:
                    raise ValueError(
                        f"field '{self.name}' with count_from=remaining "
                        f"requires element_type"
                    )
                if _try_element_byte_size(self.element_type) is None:
                    raise ValueError(
                        f"field '{self.name}' with count_from=remaining "
                        f"requires an element with a statically-known "
                        f"byte size (a primitive type, or an inline "
                        f"descriptor with size_bytes for fixed_string / "
                        f"fixed_bytes). Struct references by name are "
                        f"not yet resolvable at schema-validation time."
                    )
        return self


# Resolve ElementDescriptor.fields's forward reference to FieldSchema.
# Passing an explicit types namespace makes the resolution independent
# of the caller's module globals, which matters because
# ElementDescriptor was defined in a different module from FieldSchema.
# Without this, a schema whose elements are structs with named sub-fields
# (e.g. POINT, TRAJECTORY, TDATA) would fail validation with
# "ElementDescriptor is not fully defined".
ElementDescriptor.model_rebuild(_types_namespace={"FieldSchema": FieldSchema})
