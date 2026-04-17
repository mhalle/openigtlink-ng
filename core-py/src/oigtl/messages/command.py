# GENERATED from spec/schemas/command.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""COMMAND message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields

_FIELDS = [   {'name': 'command_id', 'type': 'uint32'},
    {   'name': 'command_name',
        'type': 'fixed_string',
        'size_bytes': 128,
        'encoding': 'ascii',
        'null_padded': True},
    {'name': 'encoding', 'type': 'uint16'},
    {'name': 'length', 'type': 'uint32'},
    {   'name': 'command',
        'type': 'array',
        'element_type': 'uint8',
        'count': 'length'}]



class Command(BaseModel):

    TYPE_ID: ClassVar[str] = "COMMAND"


    command_id: int = 0
    command_name: str = ""
    encoding: int = 0
    length: int = 0
    command: bytes = b""


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Command":
        """Decode wire body bytes into a :class:`Command` instance."""
        instance = cls.model_validate(unpack_fields(_FIELDS, data))
        return instance
