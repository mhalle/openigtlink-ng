# GENERATED from spec/schemas/capability.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""CAPABILITY message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, ClassVar

from pydantic import BaseModel, Field

from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields


_FIELDS = [   {   'name': 'supported_types',
        'type': 'array',
        'element_type': {   'type': 'fixed_string',
                            'size_bytes': 12,
                            'encoding': 'ascii',
                            'null_padded': True},
        'count_from': 'remaining'}]



class Capability(BaseModel):
    TYPE_ID: ClassVar[str] = "CAPABILITY"


    supported_types: list[str] = Field(default_factory=list)

    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Capability":
        """Decode wire body bytes into a :class:`Capability` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
