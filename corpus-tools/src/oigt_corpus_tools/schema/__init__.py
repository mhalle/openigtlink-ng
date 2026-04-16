"""Pydantic models and JSON Schema emission for message schemas.

The Pydantic models in :mod:`oigt_corpus_tools.schema.model` are the
source of truth for what a file under ``spec/schemas/`` may contain.
The JSON Schema in ``spec/meta-schema.json`` is generated from those
models via :func:`generate_meta_schema` and the
``oigt-corpus schema emit-meta`` CLI subcommand; it exists as a spec
artifact for non-Python consumers, not as an authoritative specification.
"""

from oigt_corpus_tools.schema.emit import (
    CompactJsonSchema,
    generate_meta_schema,
)
from oigt_corpus_tools.schema.model import (
    CountSource,
    ElementDescriptor,
    Encoding,
    Endianness,
    ExtendedHeader,
    FieldSchema,
    FieldType,
    MessageSchema,
    PrimitiveType,
    ProtocolVersion,
    SpecReference,
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
