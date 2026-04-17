# GENERATED from spec/schemas/stt_video.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""STT_VIDEO message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, ClassVar

from pydantic import BaseModel, Field

from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields


_FIELDS = [   {   'name': 'codec',
        'type': 'fixed_string',
        'size_bytes': 4,
        'encoding': 'ascii',
        'null_padded': False},
    {'name': 'time_interval', 'type': 'uint32'}]



class SttVideo(BaseModel):
    TYPE_ID: ClassVar[str] = "STT_VIDEO"
    BODY_SIZE: ClassVar[int] = 8


    codec: str = ""
    time_interval: int = 0

    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "SttVideo":
        """Decode wire body bytes into a :class:`SttVideo` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
