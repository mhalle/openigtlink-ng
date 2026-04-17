# GENERATED from spec/schemas/qtrans.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""QTRANS message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, ClassVar

from pydantic import BaseModel, Field

from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields


_FIELDS = [   {   'name': 'position',
        'type': 'array',
        'element_type': 'float32',
        'count': 3},
    {   'name': 'quaternion',
        'type': 'array',
        'element_type': 'float32',
        'count': 4}]



class Qtrans(BaseModel):
    TYPE_ID: ClassVar[str] = "QTRANS"
    BODY_SIZE: ClassVar[int] = 28


    position: Annotated[list[float], Field(min_length=3, max_length=3)] = Field(default_factory=lambda: [0.0] * 3, min_length=3, max_length=3)
    quaternion: Annotated[list[float], Field(min_length=4, max_length=4)] = Field(default_factory=lambda: [0.0] * 4, min_length=4, max_length=4)

    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Qtrans":
        """Decode wire body bytes into a :class:`Qtrans` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
