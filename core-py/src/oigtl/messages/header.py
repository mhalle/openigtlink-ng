# GENERATED from spec/schemas/header.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""HEADER message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields

_FIELDS = [   {'name': 'version', 'type': 'uint16'},
    {   'name': 'type',
        'type': 'fixed_string',
        'size_bytes': 12,
        'encoding': 'ascii',
        'null_padded': True},
    {   'name': 'device_name',
        'type': 'fixed_string',
        'size_bytes': 20,
        'encoding': 'ascii',
        'null_padded': True},
    {'name': 'timestamp', 'type': 'uint64'},
    {'name': 'body_size', 'type': 'uint64'},
    {'name': 'crc', 'type': 'uint64'}]



class Header(BaseModel):

    TYPE_ID: ClassVar[str] = "HEADER"
    BODY_SIZE: ClassVar[int] = 58


    version: int = 0
    type: str = ""
    device_name: str = ""
    timestamp: int = 0
    body_size: int = 0
    crc: int = 0


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Header":
        """Decode wire body bytes into a :class:`Header` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
