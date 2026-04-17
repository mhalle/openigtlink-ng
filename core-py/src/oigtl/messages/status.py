# GENERATED from spec/schemas/status.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""STATUS message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields

_FIELDS = [   {'name': 'code', 'type': 'uint16'},
    {'name': 'subcode', 'type': 'int64'},
    {   'name': 'error_name',
        'type': 'fixed_string',
        'size_bytes': 20,
        'encoding': 'ascii',
        'null_padded': True},
    {   'name': 'status_message',
        'type': 'trailing_string',
        'encoding': 'ascii',
        'null_terminated': True}]



class Status(BaseModel):

    TYPE_ID: ClassVar[str] = "STATUS"


    code: int = 0
    subcode: int = 0
    error_name: str = ""
    status_message: str = ""


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Status":
        """Decode wire body bytes into a :class:`Status` instance."""
        instance = cls.model_validate(unpack_fields(_FIELDS, data))
        return instance
