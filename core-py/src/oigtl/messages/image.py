# GENERATED from spec/schemas/image.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""IMAGE message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields
from oigtl_corpus_tools.codec.policy import POST_UNPACK_INVARIANTS

_FIELDS = [   {'name': 'header_version', 'type': 'uint16'},
    {'name': 'num_components', 'type': 'uint8'},
    {'name': 'scalar_type', 'type': 'uint8'},
    {'name': 'endian', 'type': 'uint8'},
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
    {   'name': 'pixels',
        'type': 'array',
        'element_type': 'uint8',
        'count_from': 'remaining'}]



class Image(BaseModel):

    TYPE_ID: ClassVar[str] = "IMAGE"


    header_version: int = 0
    num_components: int = 0
    scalar_type: int = 0
    endian: int = 0
    coord: int = 0
    size: Annotated[list[int], Field(min_length=3, max_length=3)] = Field(default_factory=lambda: [0] * 3, min_length=3, max_length=3)
    matrix: Annotated[list[float], Field(min_length=12, max_length=12)] = Field(default_factory=lambda: [0.0] * 12, min_length=12, max_length=12)
    subvol_offset: Annotated[list[int], Field(min_length=3, max_length=3)] = Field(default_factory=lambda: [0] * 3, min_length=3, max_length=3)
    subvol_size: Annotated[list[int], Field(min_length=3, max_length=3)] = Field(default_factory=lambda: [0] * 3, min_length=3, max_length=3)
    pixels: bytes = b""


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Image":
        """Decode wire body bytes into a :class:`Image` instance."""
        instance = cls.model_validate(unpack_fields(_FIELDS, data))
        POST_UNPACK_INVARIANTS["image"](instance)
        return instance
