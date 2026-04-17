# GENERATED from spec/schemas/get_traj.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""GET_TRAJ message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields

_FIELDS = []



class GetTraj(BaseModel):

    TYPE_ID: ClassVar[str] = "GET_TRAJ"
    BODY_SIZE: ClassVar[int] = 0




    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "GetTraj":
        """Decode wire body bytes into a :class:`GetTraj` instance."""
        instance = cls.model_validate(unpack_fields(_FIELDS, data))
        return instance
