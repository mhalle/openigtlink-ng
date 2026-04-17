# GENERATED from spec/schemas/stt_tdata.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""STT_TDATA message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, ClassVar

from pydantic import BaseModel, Field

from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields


_FIELDS = [   {'name': 'resolution', 'type': 'int32'},
    {   'name': 'coord_name',
        'type': 'fixed_string',
        'size_bytes': 32,
        'encoding': 'ascii',
        'null_padded': True}]



class SttTdata(BaseModel):
    TYPE_ID: ClassVar[str] = "STT_TDATA"
    BODY_SIZE: ClassVar[int] = 36


    resolution: int = 0
    coord_name: str = ""

    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "SttTdata":
        """Decode wire body bytes into a :class:`SttTdata` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
