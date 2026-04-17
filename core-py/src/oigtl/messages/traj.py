# GENERATED from spec/schemas/traj.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""TRAJ message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields

_FIELDS = [   {   'name': 'trajectories',
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
                                          {'name': 'type', 'type': 'int8'},
                                          {   'name': 'reserved',
                                              'type': 'int8'},
                                          {   'name': 'rgba',
                                              'type': 'array',
                                              'element_type': 'uint8',
                                              'count': 4},
                                          {   'name': 'entry_pos',
                                              'type': 'array',
                                              'element_type': 'float32',
                                              'count': 3},
                                          {   'name': 'target_pos',
                                              'type': 'array',
                                              'element_type': 'float32',
                                              'count': 3},
                                          {   'name': 'radius',
                                              'type': 'float32'},
                                          {   'name': 'owner_name',
                                              'type': 'fixed_string',
                                              'size_bytes': 20,
                                              'encoding': 'ascii',
                                              'null_padded': True}]},
        'count_from': 'remaining'}]


class _Trajectory(BaseModel):
    name: str = ""
    group_name: str = ""
    type: int = 0
    reserved: int = 0
    rgba: Annotated[bytes, Field(min_length=4, max_length=4)] = Field(default=bytes(4), min_length=4, max_length=4)
    entry_pos: Annotated[list[float], Field(min_length=3, max_length=3)] = Field(default_factory=lambda: [0.0] * 3, min_length=3, max_length=3)
    target_pos: Annotated[list[float], Field(min_length=3, max_length=3)] = Field(default_factory=lambda: [0.0] * 3, min_length=3, max_length=3)
    radius: float = 0.0
    owner_name: str = ""

class Traj(BaseModel):

    TYPE_ID: ClassVar[str] = "TRAJ"

    Trajectory: ClassVar[type[BaseModel]] = _Trajectory

    trajectories: list["Trajectory"] = Field(default_factory=list)


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Traj":
        """Decode wire body bytes into a :class:`Traj` instance."""
        instance = cls.model_validate(unpack_fields(_FIELDS, data))
        return instance
