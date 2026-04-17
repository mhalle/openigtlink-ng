# GENERATED from spec/schemas/colort.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""COLORT message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields

_FIELDS = [   {'name': 'index_type', 'type': 'int8'},
    {'name': 'map_type', 'type': 'int8'},
    {   'name': 'table',
        'type': 'array',
        'element_type': 'uint8',
        'count_from': 'remaining'}]



class Colort(BaseModel):

    TYPE_ID: ClassVar[str] = "COLORT"


    index_type: int = 0
    map_type: int = 0
    table: bytes = b""


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Colort":
        """Decode wire body bytes into a :class:`Colort` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
