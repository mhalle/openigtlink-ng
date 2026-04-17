# GENERATED from spec/schemas/sensor.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""SENSOR message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field, field_validator
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields
from oigtl.runtime.arrays import (
    coerce_variable_array,
    empty_variable_array,
    pack_variable_array,
)

_FIELDS = [   {'name': 'larray', 'type': 'uint8'},
    {'name': 'status', 'type': 'uint8'},
    {'name': 'unit', 'type': 'uint64'},
    {   'name': 'data',
        'type': 'array',
        'element_type': 'float64',
        'count': 'larray'}]



class Sensor(BaseModel):
    model_config = ConfigDict(arbitrary_types_allowed=True)

    TYPE_ID: ClassVar[str] = "SENSOR"


    larray: int = 0
    status: int = 0
    unit: int = 0
    data: Any = Field(default_factory=lambda: empty_variable_array('float64'))

    @field_validator("data", mode="before")
    @classmethod
    def _coerce_data(cls, v: Any) -> Any:
        return coerce_variable_array(v, "float64")


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        data = self.model_dump()
        data["data"] = pack_variable_array(
            self.data, "float64"
        )
        return pack_fields(_FIELDS, data)

    @classmethod
    def unpack(cls, data: bytes) -> "Sensor":
        """Decode wire body bytes into a :class:`Sensor` instance."""
        instance = cls.model_validate(unpack_fields(_FIELDS, data))
        return instance
