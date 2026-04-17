# GENERATED from spec/schemas/point.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""POINT message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, ClassVar

from pydantic import BaseModel, Field

from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields


_FIELDS = [   {   'name': 'points',
        'type': 'array',
        'element_type': {   'type': 'struct',
                            'fields': [   {   'name': 'name',
                                              'type': 'fixed_string',
                                              'size_bytes': 64,
                                              'encoding': 'ascii',
                                              'null_padded': True},
                                          {   'name': 'group_name',
                                              'type': 'fixed_string',
                                              'size_bytes': 32,
                                              'encoding': 'ascii',
                                              'null_padded': True},
                                          {   'name': 'rgba',
                                              'type': 'array',
                                              'element_type': 'uint8',
                                              'count': 4},
                                          {   'name': 'position',
                                              'type': 'array',
                                              'element_type': 'float32',
                                              'count': 3},
                                          {   'name': 'radius',
                                              'type': 'float32'},
                                          {   'name': 'owner',
                                              'type': 'fixed_string',
                                              'size_bytes': 20,
                                              'encoding': 'ascii',
                                              'null_padded': True}]},
        'count_from': 'remaining'}]


class _PointEntry(BaseModel):
    name: str = ""
    group_name: str = ""
    rgba: Annotated[bytes, Field(min_length=4, max_length=4)] = Field(default=bytes(4), min_length=4, max_length=4)
    position: Annotated[list[float], Field(min_length=3, max_length=3)] = Field(default_factory=lambda: [0.0] * 3, min_length=3, max_length=3)
    radius: float = 0.0
    owner: str = ""

class Point(BaseModel):
    TYPE_ID: ClassVar[str] = "POINT"

    PointEntry: ClassVar[type[BaseModel]] = _PointEntry

    points: list["PointEntry"] = Field(default_factory=list)

    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Point":
        """Decode wire body bytes into a :class:`Point` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
