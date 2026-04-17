# GENERATED from spec/schemas/position.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""POSITION message — typed Python wire codec."""

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
        'count_from': 'remaining'}]



class Position(BaseModel):
    TYPE_ID: ClassVar[str] = "POSITION"


    position: Annotated[list[float], Field(min_length=3, max_length=3)] = Field(default_factory=lambda: [0.0] * 3, min_length=3, max_length=3)
    quaternion: list[float] = Field(default_factory=list)

    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Position":
        """Decode wire body bytes into a :class:`Position` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
