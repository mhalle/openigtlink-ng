# GENERATED from spec/schemas/stp_qtrans.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""STP_QTRANS message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields

_FIELDS = []



class StpQtrans(BaseModel):

    TYPE_ID: ClassVar[str] = "STP_QTRANS"
    BODY_SIZE: ClassVar[int] = 0




    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "StpQtrans":
        """Decode wire body bytes into a :class:`StpQtrans` instance."""
        instance = cls.model_validate(unpack_fields(_FIELDS, data))
        return instance
