# GENERATED from spec/schemas/stt_bind.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""STT_BIND message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, ClassVar

from pydantic import BaseModel, Field

from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields


_FIELDS = [   {'name': 'resolution', 'type': 'uint64'},
    {'name': 'ncmessages', 'type': 'uint16'},
    {   'name': 'type_ids',
        'type': 'array',
        'element_type': {   'type': 'fixed_string',
                            'size_bytes': 12,
                            'encoding': 'ascii',
                            'null_padded': True},
        'count': 'ncmessages'},
    {'name': 'nametable_size', 'type': 'uint16'},
    {   'name': 'name_table',
        'type': 'array',
        'element_type': 'uint8',
        'count': 'nametable_size'}]



class SttBind(BaseModel):
    TYPE_ID: ClassVar[str] = "STT_BIND"


    resolution: int = 0
    ncmessages: int = 0
    type_ids: list[str] = Field(default_factory=list)
    nametable_size: int = 0
    name_table: bytes = b""

    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "SttBind":
        """Decode wire body bytes into a :class:`SttBind` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
