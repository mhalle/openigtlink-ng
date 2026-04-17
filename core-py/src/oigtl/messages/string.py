# GENERATED from spec/schemas/string.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""STRING message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields

_FIELDS = [   {'name': 'encoding', 'type': 'uint16'},
    {   'name': 'value',
        'type': 'length_prefixed_string',
        'length_prefix_type': 'uint16',
        'encoding': 'binary'}]



class String(BaseModel):

    TYPE_ID: ClassVar[str] = "STRING"


    encoding: int = 0
    value: bytes = b""


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "String":
        """Decode wire body bytes into a :class:`String` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
