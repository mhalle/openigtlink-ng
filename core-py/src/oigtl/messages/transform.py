# GENERATED from spec/schemas/transform.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""TRANSFORM message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields

_FIELDS = [   {   'name': 'matrix',
        'type': 'array',
        'element_type': 'float32',
        'count': 12,
        'size_bytes': 48}]



class Transform(BaseModel):

    TYPE_ID: ClassVar[str] = "TRANSFORM"
    BODY_SIZE: ClassVar[int] = 48


    matrix: Annotated[list[float], Field(min_length=12, max_length=12)] = Field(default_factory=lambda: [0.0] * 12, min_length=12, max_length=12)


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Transform":
        """Decode wire body bytes into a :class:`Transform` instance."""
        instance = cls.model_validate(unpack_fields(_FIELDS, data))
        return instance
