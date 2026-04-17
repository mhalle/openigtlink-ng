# GENERATED from spec/schemas/video.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""VIDEO message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields

_FIELDS = [   {'name': 'header_version', 'type': 'uint16'},
    {'name': 'endian', 'type': 'uint8'},
    {   'name': 'codec',
        'type': 'fixed_string',
        'size_bytes': 4,
        'encoding': 'ascii',
        'null_padded': False},
    {'name': 'frame_type', 'type': 'uint16'},
    {'name': 'coord', 'type': 'uint8'},
    {'name': 'size', 'type': 'array', 'element_type': 'uint16', 'count': 3},
    {   'name': 'matrix',
        'type': 'array',
        'element_type': 'float32',
        'count': 12},
    {   'name': 'subvol_offset',
        'type': 'array',
        'element_type': 'uint16',
        'count': 3},
    {   'name': 'subvol_size',
        'type': 'array',
        'element_type': 'uint16',
        'count': 3},
    {   'name': 'frame_data',
        'type': 'array',
        'element_type': 'uint8',
        'count_from': 'remaining'}]



class Video(BaseModel):

    TYPE_ID: ClassVar[str] = "VIDEO"


    header_version: int = 0
    endian: int = 0
    codec: str = ""
    frame_type: int = 0
    coord: int = 0
    size: Annotated[list[int], Field(min_length=3, max_length=3)] = Field(default_factory=lambda: [0] * 3, min_length=3, max_length=3)
    matrix: Annotated[list[float], Field(min_length=12, max_length=12)] = Field(default_factory=lambda: [0.0] * 12, min_length=12, max_length=12)
    subvol_offset: Annotated[list[int], Field(min_length=3, max_length=3)] = Field(default_factory=lambda: [0] * 3, min_length=3, max_length=3)
    subvol_size: Annotated[list[int], Field(min_length=3, max_length=3)] = Field(default_factory=lambda: [0] * 3, min_length=3, max_length=3)
    frame_data: bytes = b""


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Video":
        """Decode wire body bytes into a :class:`Video` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
