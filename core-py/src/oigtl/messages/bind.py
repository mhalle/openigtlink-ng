# GENERATED from spec/schemas/bind.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""BIND message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, ClassVar

from pydantic import BaseModel, Field

from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields


_FIELDS = [   {'name': 'ncmessages', 'type': 'uint16'},
    {   'name': 'header_entries',
        'type': 'array',
        'element_type': {   'type': 'struct',
                            'fields': [   {   'name': 'type_id',
                                              'type': 'fixed_string',
                                              'size_bytes': 12,
                                              'encoding': 'ascii',
                                              'null_padded': True},
                                          {   'name': 'body_size',
                                              'type': 'uint64'}]},
        'count': 'ncmessages'},
    {'name': 'nametable_size', 'type': 'uint16'},
    {   'name': 'name_table',
        'type': 'array',
        'element_type': 'uint8',
        'count': 'nametable_size'},
    {   'name': 'bodies',
        'type': 'array',
        'element_type': 'uint8',
        'count_from': 'remaining'}]


class _HeaderEntry(BaseModel):
    type_id: str = ""
    body_size: int = 0

class Bind(BaseModel):
    TYPE_ID: ClassVar[str] = "BIND"

    HeaderEntry: ClassVar[type[BaseModel]] = _HeaderEntry

    ncmessages: int = 0
    header_entries: list["HeaderEntry"] = Field(default_factory=list)
    nametable_size: int = 0
    name_table: bytes = b""
    bodies: bytes = b""

    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Bind":
        """Decode wire body bytes into a :class:`Bind` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
