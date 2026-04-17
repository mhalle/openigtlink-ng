# GENERATED from spec/schemas/tdata.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""TDATA message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields

_FIELDS = [   {   'name': 'tools',
        'type': 'array',
        'element_type': {   'type': 'struct',
                            'fields': [   {   'name': 'name',
                                              'type': 'fixed_string',
                                              'size_bytes': 20,
                                              'encoding': 'ascii',
                                              'null_padded': True},
                                          {'name': 'type', 'type': 'uint8'},
                                          {   'name': 'reserved',
                                              'type': 'uint8'},
                                          {   'name': 'transform',
                                              'type': 'array',
                                              'element_type': 'float32',
                                              'count': 12}]},
        'count_from': 'remaining'}]


class _Tool(BaseModel):
    name: str = ""
    type: int = 0
    reserved: int = 0
    transform: Annotated[list[float], Field(min_length=12, max_length=12)] = Field(default_factory=lambda: [0.0] * 12, min_length=12, max_length=12)

class Tdata(BaseModel):

    TYPE_ID: ClassVar[str] = "TDATA"

    Tool: ClassVar[type[BaseModel]] = _Tool

    tools: list["Tool"] = Field(default_factory=list)


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Tdata":
        """Decode wire body bytes into a :class:`Tdata` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
