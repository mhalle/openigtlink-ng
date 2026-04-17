# GENERATED from spec/schemas/polydata.json — do not edit.
#
# Regenerate with: uv run oigtl-corpus codegen python
"""POLYDATA message — typed Python wire codec."""

from __future__ import annotations

from typing import Annotated, Any, ClassVar

from pydantic import BaseModel, ConfigDict, Field
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields
from oigtl_corpus_tools.codec.policy import POST_UNPACK_INVARIANTS

_FIELDS = [   {'name': 'npoints', 'type': 'uint32'},
    {'name': 'nvertices', 'type': 'uint32'},
    {'name': 'size_vertices', 'type': 'uint32'},
    {'name': 'nlines', 'type': 'uint32'},
    {'name': 'size_lines', 'type': 'uint32'},
    {'name': 'npolygons', 'type': 'uint32'},
    {'name': 'size_polygons', 'type': 'uint32'},
    {'name': 'ntriangle_strips', 'type': 'uint32'},
    {'name': 'size_triangle_strips', 'type': 'uint32'},
    {'name': 'nattributes', 'type': 'uint32'},
    {   'name': 'points',
        'type': 'array',
        'element_type': {   'type': 'struct',
                            'fields': [   {'name': 'x', 'type': 'float32'},
                                          {'name': 'y', 'type': 'float32'},
                                          {'name': 'z', 'type': 'float32'}]},
        'count': 'npoints'},
    {   'name': 'vertices',
        'type': 'array',
        'element_type': 'uint8',
        'count': 'size_vertices'},
    {   'name': 'lines',
        'type': 'array',
        'element_type': 'uint8',
        'count': 'size_lines'},
    {   'name': 'polygons',
        'type': 'array',
        'element_type': 'uint8',
        'count': 'size_polygons'},
    {   'name': 'triangle_strips',
        'type': 'array',
        'element_type': 'uint8',
        'count': 'size_triangle_strips'},
    {   'name': 'attribute_headers',
        'type': 'array',
        'element_type': {   'type': 'struct',
                            'fields': [   {'name': 'type', 'type': 'uint8'},
                                          {   'name': 'ncomponents',
                                              'type': 'uint8'},
                                          {'name': 'n', 'type': 'uint32'}]},
        'count': 'nattributes'},
    {   'name': 'attribute_data',
        'type': 'array',
        'element_type': 'uint8',
        'count_from': 'remaining'}]


class _Point(BaseModel):
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0
class _AttributeHeader(BaseModel):
    type: int = 0
    ncomponents: int = 0
    n: int = 0

class Polydata(BaseModel):

    TYPE_ID: ClassVar[str] = "POLYDATA"

    Point: ClassVar[type[BaseModel]] = _Point
    AttributeHeader: ClassVar[type[BaseModel]] = _AttributeHeader

    npoints: int = 0
    nvertices: int = 0
    size_vertices: int = 0
    nlines: int = 0
    size_lines: int = 0
    npolygons: int = 0
    size_polygons: int = 0
    ntriangle_strips: int = 0
    size_triangle_strips: int = 0
    nattributes: int = 0
    points: list["Point"] = Field(default_factory=list)
    vertices: bytes = b""
    lines: bytes = b""
    polygons: bytes = b""
    triangle_strips: bytes = b""
    attribute_headers: list["AttributeHeader"] = Field(default_factory=list)
    attribute_data: bytes = b""


    def pack(self) -> bytes:
        """Serialize this message's body to wire bytes."""
        return pack_fields(_FIELDS, self.model_dump())

    @classmethod
    def unpack(cls, data: bytes) -> "Polydata":
        """Decode wire body bytes into a :class:`Polydata` instance."""
        instance = cls.model_validate(unpack_fields(_FIELDS, data))
        POST_UNPACK_INVARIANTS["polydata"](instance)
        return instance
