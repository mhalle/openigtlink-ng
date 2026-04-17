# GENERATED from spec/schemas/position.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""POSITION message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field, field_validator
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields
from oigtl.runtime.arrays import (
    coerce_variable_array,
    empty_variable_array,
    pack_variable_array,
)

_FIELDS = [   {   'name': 'position',
        'type': 'array',
        'element_type': 'float32',
        'count': 3},
    {   'name': 'quaternion',
        'type': 'array',
        'element_type': 'float32',
        'count_from': 'remaining'}]



class Position(BaseModel):
    model_config = ConfigDict(arbitrary_types_allowed=True)

    TYPE_ID: ClassVar[str] = "POSITION"


    position: Annotated[list[float], Field(min_length=3, max_length=3)] = Field(default_factory=lambda: [0.0] * 3, min_length=3, max_length=3)
    quaternion: Any = Field(default_factory=lambda: empty_variable_array('float32'))

    @field_validator("quaternion", mode="before")
    @classmethod
    def _coerce_quaternion(cls, v: Any) -> Any:
        return coerce_variable_array(v, "float32")


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        data = self.model_dump()
        data["quaternion"] = pack_variable_array(
            self.quaternion, "float32"
        )
        return pack_fields(_FIELDS, data)

    @classmethod
    def unpack(cls, data: bytes) -> "Position":
        """Decode wire body bytes into a :class:`Position` instance."""
        if len(data) not in [12, 24, 28]:
            raise ValueError(
                "POSITION body_size=" + str(len(data))
                + " is not in the allowed set [12, 24, 28]"
            )
        return cls.model_validate(unpack_fields(_FIELDS, data))
