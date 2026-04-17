# GENERATED from spec/schemas/rts_image.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""RTS_IMAGE message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields

_FIELDS = [{'name': 'status', 'type': 'int8'}]



class RtsImage(BaseModel):

    TYPE_ID: ClassVar[str] = "RTS_IMAGE"
    BODY_SIZE: ClassVar[int] = 1


    status: int = 0


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "RtsImage":
        """Decode wire body bytes into a :class:`RtsImage` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
