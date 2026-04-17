# GENERATED from spec/schemas/rts_imgmeta.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""RTS_IMGMETA message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields

_FIELDS = [{'name': 'status', 'type': 'int8'}]



class RtsImgmeta(BaseModel):

    TYPE_ID: ClassVar[str] = "RTS_IMGMETA"
    BODY_SIZE: ClassVar[int] = 1


    status: int = 0


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "RtsImgmeta":
        """Decode wire body bytes into a :class:`RtsImgmeta` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
