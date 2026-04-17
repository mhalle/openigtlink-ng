# GENERATED from spec/schemas/sensor.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""SENSOR message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, ClassVar

from pydantic import BaseModel, Field

from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields


_FIELDS = [   {'name': 'larray', 'type': 'uint8'},
    {'name': 'status', 'type': 'uint8'},
    {'name': 'unit', 'type': 'uint64'},
    {   'name': 'data',
        'type': 'array',
        'element_type': 'float64',
        'count': 'larray'}]



class Sensor(BaseModel):
    TYPE_ID: ClassVar[str] = "SENSOR"


    larray: int = 0
    status: int = 0
    unit: int = 0
    data: list[float] = Field(default_factory=list)

    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Sensor":
        """Decode wire body bytes into a :class:`Sensor` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
