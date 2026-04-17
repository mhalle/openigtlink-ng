"""Top-level message schema and its small companion models.

This module defines the three Pydantic classes that together describe a
complete message-type entry:

- :class:`SpecReference` — back-reference into the prose spec document.
- :class:`ExtendedHeader` — v3 extended-header layout, if non-default.
- :class:`MessageSchema` — the root object validated against every file
  under ``spec/schemas/``.

:class:`MessageSchema` carries the one cross-field rule that cannot live
at field level: the ``trailing_string`` field, if present, must be the
last field in its message.
"""

from __future__ import annotations

from typing import Literal, Optional

from pydantic import BaseModel, ConfigDict, Field, model_validator

from oigtl_corpus_tools.schema.field import FieldSchema
from oigtl_corpus_tools.schema.types import (
    FieldType,
    ProtocolVersion,
    Size,
    TypeIdString,
    UpperIdentifier,
)


# ---------------------------------------------------------------------------
# Spec reference
# ---------------------------------------------------------------------------


class SpecReference(BaseModel):
    """Back-reference from a schema or field into the prose spec document."""

    model_config = ConfigDict(extra="forbid")

    section: str = Field(
        description=(
            "Section identifier in the prose spec document (e.g. '4.3.2' "
            "or 'Body (STATUS)')."
        ),
    )
    document: Optional[str] = Field(
        default=None,
        description=(
            "Prose spec document this entry references "
            "(e.g. 'protocol/v3.md'). Defaults to the document matching "
            "``introduced_in``."
        ),
    )
    url: Optional[str] = Field(
        default=None,
        description="Optional canonical URL for the spec section.",
        json_schema_extra={"format": "uri"},
    )


# ---------------------------------------------------------------------------
# Extended header (v3)
# ---------------------------------------------------------------------------


class ExtendedHeader(BaseModel):
    """V3 extended-header layout, if non-default for a message type."""

    model_config = ConfigDict(extra="forbid")

    required: Optional[bool] = Field(
        default=None,
        description=(
            "Whether the v3 extended header is required for this message "
            "type."
        ),
    )
    fields: Optional[list[FieldSchema]] = Field(
        default=None,
        description="Extended-header field layout, if non-default.",
    )


# ---------------------------------------------------------------------------
# Message
# ---------------------------------------------------------------------------


class MessageSchema(BaseModel):
    """Top-level schema describing one OpenIGTLink message type."""

    model_config = ConfigDict(extra="forbid")

    message_type: UpperIdentifier = Field(
        description=(
            "Canonical logical name for this message type (e.g. "
            "'TRANSFORM'). Used to construct language-native type names "
            "during codegen."
        ),
    )
    type_id: TypeIdString = Field(
        description=(
            "Wire type identifier. Written as a 12-byte ASCII field, "
            "null-padded on the right. The string value here contains the "
            "significant bytes only; padding is applied during encoding."
        ),
    )
    introduced_in: ProtocolVersion = Field(
        description=(
            "Protocol version in which this message type was first "
            "defined."
        ),
    )
    deprecated_in: Optional[Literal["v2", "v3", "v4"]] = Field(
        default=None,
        description=(
            "Protocol version in which this message type was deprecated, "
            "if any. Deprecated types must still round-trip correctly for "
            "wire compatibility."
        ),
    )
    description: str = Field(
        min_length=1,
        description=(
            "Concise human-readable summary. Required. Flows into every "
            "language port's generated API documentation."
        ),
    )
    rationale: Optional[str] = Field(
        default=None,
        description=(
            "Optional prose explaining why the message type or its wire "
            "layout is shaped this way. Historical or design-decision "
            "context, not behavioral contract."
        ),
    )
    spec_reference: Optional[SpecReference] = Field(
        default=None,
        description="Back-reference into the prose spec document.",
    )
    body_size: Size = Field(
        description=(
            "Total body size in bytes if fixed, or the string 'variable' "
            "if determined by the fields at encode time."
        ),
    )
    body_size_set: Optional[list[int]] = Field(
        default=None,
        description=(
            "When ``body_size`` is 'variable' but the spec restricts it "
            "to a finite set of legal values, list them here. Codecs "
            "MUST reject any body whose length is not in this set "
            "before any field access. Used by POSITION (12, 24, 28)."
        ),
    )
    fields: list[FieldSchema] = Field(
        description=(
            "Ordered list of fields in the message body. Fields are "
            "parsed in order."
        ),
    )
    extended_header: Optional[ExtendedHeader] = Field(
        default=None,
        description=(
            "V3 extended-header layout for this message type, if "
            "non-default."
        ),
    )
    post_unpack_invariant: Optional[str] = Field(
        default=None,
        description=(
            "Name of a cross-field invariant every codec must enforce "
            "after unpacking, selected from a closed vocabulary defined "
            "in ``codec/policy.py::POST_UNPACK_INVARIANTS``. These are "
            "constraints that depend on more than one field's value "
            "(e.g. ``len(data) == prod(size) * scalar_bytes(scalar_type)`` "
            "for NDARRAY) and therefore cannot be expressed as a per-"
            "field type. Four distinct codec runtimes implement the same "
            "named invariant; the differential fuzzer keeps them aligned."
        ),
    )
    metadata_allowed: Optional[bool] = Field(
        default=None,
        description=(
            "Whether this message type accepts a trailing metadata block "
            "in v2/v3. Defaults to true."
        ),
    )
    legacy_notes: Optional[list[str]] = Field(
        default=None,
        description=(
            "Historical quirks that must be preserved for wire "
            "compatibility. These are contract, not documentation — every "
            "entry represents a decision that a new implementation must "
            "match. Prefix with a version tag where relevant "
            "(e.g. 'v2.1:')."
        ),
    )
    notes: Optional[list[str]] = Field(
        default=None,
        description=(
            "Other prose notes that do not fit rationale, spec_reference, "
            "or legacy_notes."
        ),
    )
    see_also: Optional[list[UpperIdentifier]] = Field(
        default=None,
        description="Logical names of related message types.",
    )

    @model_validator(mode="after")
    def _check_trailing_string_is_last(self) -> MessageSchema:
        for i, f in enumerate(self.fields):
            if f.type == FieldType.TRAILING_STRING and i != len(self.fields) - 1:
                raise ValueError(
                    f"trailing_string field '{f.name}' must be the last "
                    f"field in the message (found at index {i}, with "
                    f"{len(self.fields) - i - 1} field(s) following)"
                )
        return self
