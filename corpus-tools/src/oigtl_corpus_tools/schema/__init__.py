"""Pydantic models and JSON Schema emission for message schemas.

The Pydantic models in this package are the source of truth for what a
file under ``spec/schemas/`` may contain. The JSON Schema in
``spec/meta-schema.json`` is generated from those models via
:func:`generate_meta_schema` and the ``oigtl-corpus schema emit-meta``
CLI subcommand; it exists as a spec artifact for non-Python consumers,
not as an authoritative specification.

Module layout:

- :mod:`.types` — enums and constrained type aliases, no Pydantic
  models.
- :mod:`.element` — :class:`ElementDescriptor` for inline array-element
  shape.
- :mod:`.field` — :class:`FieldSchema` plus the byte-size helpers used
  by its validators; also finalizes the mutual recursion with
  :class:`ElementDescriptor`.
- :mod:`.message` — :class:`MessageSchema` (the root) plus
  :class:`SpecReference` and :class:`ExtendedHeader`.
- :mod:`.emit` — JSON Schema generator.

Future meta-schema extensions (e.g. size-discriminated unions for
POSITION, multi-section containers for BIND, nested-mesh sections for
POLYDATA) should land in their own modules rather than swelling
:mod:`.field` or :mod:`.message`.
"""

from oigtl_corpus_tools.schema.element import ElementDescriptor
from oigtl_corpus_tools.schema.emit import (
    CompactJsonSchema,
    generate_meta_schema,
)
from oigtl_corpus_tools.schema.field import FieldSchema
from oigtl_corpus_tools.schema.message import (
    ExtendedHeader,
    MessageSchema,
    SpecReference,
)
from oigtl_corpus_tools.schema.types import (
    CountSource,
    Encoding,
    Endianness,
    FieldType,
    PrimitiveType,
    ProtocolVersion,
)

__all__ = [
    "CompactJsonSchema",
    "CountSource",
    "ElementDescriptor",
    "Encoding",
    "Endianness",
    "ExtendedHeader",
    "FieldSchema",
    "FieldType",
    "MessageSchema",
    "PrimitiveType",
    "ProtocolVersion",
    "SpecReference",
    "generate_meta_schema",
]
