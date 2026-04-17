# GENERATED from spec/schemas/stt_qtdata.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""STT_QTDATA message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields

_FIELDS = [   {'name': 'resolution', 'type': 'int32'},
    {   'name': 'coord_name',
        'type': 'fixed_string',
        'size_bytes': 32,
        'encoding': 'ascii',
        'null_padded': True}]



class SttQtdata(BaseModel):

    TYPE_ID: ClassVar[str] = "STT_QTDATA"
    BODY_SIZE: ClassVar[int] = 36


    resolution: int = 0
    coord_name: str = ""


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "SttQtdata":
        """Decode wire body bytes into a :class:`SttQtdata` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
