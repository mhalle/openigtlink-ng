# GENERATED from spec/schemas/imgmeta.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""IMGMETA message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields

_FIELDS = [   {   'name': 'images',
        'type': 'array',
        'element_type': {   'type': 'struct',
                            'fields': [   {   'name': 'name',
                                              'type': 'fixed_string',
                                              'size_bytes': 64,
                                              'encoding': 'ascii',
                                              'null_padded': True},
                                          {   'name': 'device_name',
                                              'type': 'fixed_string',
                                              'size_bytes': 20,
                                              'encoding': 'ascii',
                                              'null_padded': True},
                                          {   'name': 'modality',
                                              'type': 'fixed_string',
                                              'size_bytes': 32,
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
                                          {   'name': 'timestamp',
                                              'type': 'uint64'},
                                          {   'name': 'size',
                                              'type': 'array',
                                              'element_type': 'uint16',
                                              'count': 3},
                                          {   'name': 'scalar_type',
                                              'type': 'uint8'},
                                          {   'name': 'reserved',
                                              'type': 'uint8'}]},
        'count_from': 'remaining'}]


class _Image(BaseModel):
    name: str = ""
    device_name: str = ""
    modality: str = ""
    patient_name: str = ""
    patient_id: str = ""
    timestamp: int = 0
    size: Annotated[list[int], Field(min_length=3, max_length=3)] = Field(default_factory=lambda: [0] * 3, min_length=3, max_length=3)
    scalar_type: int = 0
    reserved: int = 0

class Imgmeta(BaseModel):

    TYPE_ID: ClassVar[str] = "IMGMETA"

    Image: ClassVar[type[BaseModel]] = _Image

    images: list["Image"] = Field(default_factory=list)


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Imgmeta":
        """Decode wire body bytes into a :class:`Imgmeta` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
