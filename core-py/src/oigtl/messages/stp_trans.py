# GENERATED from spec/schemas/stp_trans.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""STP_TRANS message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, ClassVar

from pydantic import BaseModel, Field

from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields


_FIELDS = []



class StpTrans(BaseModel):
    TYPE_ID: ClassVar[str] = "STP_TRANS"
    BODY_SIZE: ClassVar[int] = 0



    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "StpTrans":
        """Decode wire body bytes into a :class:`StpTrans` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
