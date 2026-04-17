# GENERATED from spec/schemas/ndarray.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""NDARRAY message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field, field_validator
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields
from oigtl.runtime.arrays import (
    coerce_variable_array,
    empty_variable_array,
    pack_variable_array,
)

_FIELDS = [   {'name': 'scalar_type', 'type': 'uint8'},
    {'name': 'dim', 'type': 'uint8'},
    {   'name': 'size',
        'type': 'array',
        'element_type': 'uint16',
        'count': 'dim'},
    {   'name': 'data',
        'type': 'array',
        'element_type': 'uint8',
        'count_from': 'remaining'}]



class Ndarray(BaseModel):
    model_config = ConfigDict(arbitrary_types_allowed=True)

    TYPE_ID: ClassVar[str] = "NDARRAY"


    scalar_type: int = 0
    dim: int = 0
    size: Any = Field(default_factory=lambda: empty_variable_array('uint16'))
    data: bytes = b""

    @field_validator("size", mode="before")
    @classmethod
    def _coerce_size(cls, v: Any) -> Any:
        return coerce_variable_array(v, "uint16")


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        data = self.model_dump()
        data["size"] = pack_variable_array(
            self.size, "uint16"
        )
        return pack_fields(_FIELDS, data)

    @classmethod
    def unpack(cls, data: bytes) -> "Ndarray":
        """Decode wire body bytes into a :class:`Ndarray` instance."""
        return cls.model_validate(unpack_fields(_FIELDS, data))
