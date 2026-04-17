# GENERATED from spec/schemas/lbmeta.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""LBMETA message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, ClassVar

from pydantic import BaseModel, Field

from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields


_FIELDS = [   {   'name': 'labels',
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
                                          {'name': 'label', 'type': 'uint8'},
                                          {   'name': 'reserved',
                                              'type': 'uint8'},
                                          {   'name': 'rgba',
                                              'type': 'array',
                                              'element_type': 'uint8',
                                              'count': 4},
                                          {   'name': 'size',
                                              'type': 'array',
                                              'element_type': 'uint16',
                                              'count': 3},
                                          {   'name': 'owner',
                                              'type': 'fixed_string',
                                              'size_bytes': 20,
                                              'encoding': 'ascii',
                                              'null_padded': True}]},
        'count_from': 'remaining'}]


class _Label(BaseModel):
    name: str = ""
    device_name: str = ""
    label: int = 0
    reserved: int = 0
    rgba: Annotated[bytes, Field(min_length=4, max_length=4)] = Field(default=bytes(4), min_length=4, max_length=4)
    size: Annotated[list[int], Field(min_length=3, max_length=3)] = Field(default_factory=lambda: [0] * 3, min_length=3, max_length=3)
    owner: str = ""

class Lbmeta(BaseModel):
    TYPE_ID: ClassVar[str] = "LBMETA"

    Label: ClassVar[type[BaseModel]] = _Label

    labels: list["Label"] = Field(default_factory=list)

    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Lbmeta":
        """Decode wire body bytes into a :class:`Lbmeta` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
