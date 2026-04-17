# GENERATED from spec/schemas/videometa.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""VIDEOMETA message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields

_FIELDS = [   {   'name': 'videos',
        'type': 'array',
        'element_type': {   'type': 'struct',
                            'fields': [   {   'name': 'name',
                                              'type': 'fixed_string',
                                              'size_bytes': 64,
                                              'encoding': 'ascii',
                                              'null_padded': True},
                                          {   'name': 'device_name',
                                              'type': 'fixed_string',
                                              'size_bytes': 64,
                                              'encoding': 'ascii',
                                              'null_padded': True},
                                          {   'name': 'patient_name',
                                              'type': 'fixed_string',
                                              'size_bytes': 64,
                                              'encoding': 'ascii',
                                              'null_padded': True},
                                          {   'name': 'patient_id',
                                              'type': 'fixed_string',
                                              'size_bytes': 64,
                                              'encoding': 'ascii',
                                              'null_padded': True},
                                          {   'name': 'zoom_level',
                                              'type': 'int16'},
                                          {   'name': 'focal_length',
                                              'type': 'float64'},
                                          {   'name': 'size',
                                              'type': 'array',
                                              'element_type': 'uint16',
                                              'count': 3},
                                          {   'name': 'matrix',
                                              'type': 'array',
                                              'element_type': 'float32',
                                              'count': 12},
                                          {   'name': 'scalar_type',
                                              'type': 'uint8'},
                                          {   'name': 'reserved',
                                              'type': 'uint8'}]},
        'count_from': 'remaining'}]


class _Video(BaseModel):
    name: str = ""
    device_name: str = ""
    patient_name: str = ""
    patient_id: str = ""
    zoom_level: int = 0
    focal_length: float = 0.0
    size: Annotated[list[int], Field(min_length=3, max_length=3)] = Field(default_factory=lambda: [0] * 3, min_length=3, max_length=3)
    matrix: Annotated[list[float], Field(min_length=12, max_length=12)] = Field(default_factory=lambda: [0.0] * 12, min_length=12, max_length=12)
    scalar_type: int = 0
    reserved: int = 0

class Videometa(BaseModel):

    TYPE_ID: ClassVar[str] = "VIDEOMETA"

    Video: ClassVar[type[BaseModel]] = _Video

    videos: list["Video"] = Field(default_factory=list)


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Videometa":
        """Decode wire body bytes into a :class:`Videometa` instance."""
        instance = cls.model_validate(unpack_fields(_FIELDS, data))
        return instance
