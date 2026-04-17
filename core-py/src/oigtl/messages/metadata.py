# GENERATED from spec/schemas/metadata.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""METADATA message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, ClassVar

from pydantic import BaseModel, Field

from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields


_FIELDS = [   {'name': 'count', 'type': 'uint16'},
    {   'name': 'index_entries',
        'type': 'array',
        'element_type': {   'type': 'struct',
                            'fields': [   {   'name': 'key_size',
                                              'type': 'uint16'},
                                          {   'name': 'value_encoding',
                                              'type': 'uint16'},
                                          {   'name': 'value_size',
                                              'type': 'uint32'}]},
        'count': 'count'},
    {   'name': 'body',
        'type': 'array',
        'element_type': 'uint8',
        'count_from': 'remaining'}]


class _IndexEntry(BaseModel):
    key_size: int = 0
    value_encoding: int = 0
    value_size: int = 0

class Metadata(BaseModel):
    TYPE_ID: ClassVar[str] = "METADATA"

    IndexEntry: ClassVar[type[BaseModel]] = _IndexEntry

    count: int = 0
    index_entries: list["IndexEntry"] = Field(default_factory=list)
    body: bytes = b""

    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Metadata":
        """Decode wire body bytes into a :class:`Metadata` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
