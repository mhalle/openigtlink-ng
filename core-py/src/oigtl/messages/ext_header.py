# GENERATED from spec/schemas/ext_header.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""EXT_HEADER message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields

_FIELDS = [   {'name': 'ext_header_size', 'type': 'uint16'},
    {'name': 'metadata_header_size', 'type': 'uint16'},
    {'name': 'metadata_size', 'type': 'uint32'},
    {'name': 'message_id', 'type': 'uint32'}]



class ExtHeader(BaseModel):

    TYPE_ID: ClassVar[str] = "EXT_HEADER"


    ext_header_size: int = 0
    metadata_header_size: int = 0
    metadata_size: int = 0
    message_id: int = 0


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "ExtHeader":
        """Decode wire body bytes into a :class:`ExtHeader` instance."""
        instance = cls.model_validate(unpack_fields(_FIELDS, data))
        return instance
