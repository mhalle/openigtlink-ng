"""Emit the JSON Schema artifact under ``spec/meta-schema.json``.

The Pydantic models in :mod:`oigtl_corpus_tools.schema` (split across
``types``, ``element``, ``field``, and ``message``) are the source of
truth. The JSON Schema file is a derived artifact for the benefit of
non-Python consumers (editors, codegen in other languages, third-party
reviewers). The ``oigtl-corpus schema emit-meta`` CLI subcommand writes
the file; ``--check`` verifies the committed copy is in sync.

Two small customizations to Pydantic's default schema generation:

1. :class:`CompactJsonSchema` suppresses the ``{"type": "null"}``
   alternative that Pydantic emits for ``Optional[X] = None`` fields.
   In this project, optional fields are expressed via *absence* from
   ``required`` rather than by accepting an explicit ``null`` value,
   mirroring the original hand-authored schema.
2. :func:`generate_meta_schema` prepends a fixed header of ``$schema``,
   ``$id``, ``$comment`` (the GENERATED-from banner), ``title``, and
   ``description`` before Pydantic's own output. Keys that collide with
   the header are kept from the header, not from Pydantic.
"""

from __future__ import annotations

from typing import Any

from pydantic.json_schema import GenerateJsonSchema

from oigtl_corpus_tools.schema.message import MessageSchema


_META_SCHEMA_HEADER: dict[str, Any] = {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": "https://openigtlink-ng.org/spec/meta-schema.json",
    "$comment": (
        "GENERATED from the Pydantic models in "
        "corpus-tools/src/oigtl_corpus_tools/schema/ (types.py, element.py, "
        "field.py, message.py) — do not edit by hand; run "
        "'uv run oigtl-corpus schema emit-meta' to regenerate."
    ),
    "title": "OpenIGTLink Message Schema",
    "description": (
        "Meta-schema that validates the shape of every file under "
        "spec/schemas/. Each message-type schema describes one OpenIGTLink "
        "wire message and is consumed by codegen tools across all language "
        "ports."
    ),
}


class CompactJsonSchema(GenerateJsonSchema):
    """JSON Schema generator that elides the null alternative for Optionals.

    Pydantic's default emits ``{"anyOf": [<X>, {"type": "null"}]}`` for a
    field declared ``Optional[X] = None``. In this project's meta-schema,
    optional fields are expressed by omission (not present in ``required``)
    rather than by accepting explicit ``null``, so the ``null`` branch is
    redundant clutter. This subclass returns only the inner schema.

    Pydantic's runtime validator still accepts ``None`` for an Optional
    field because the field's Python type is ``Optional[X]``. Schema files
    in this project never contain explicit nulls — they omit the property
    instead — so the asymmetry between "what the schema describes" and
    "what the validator accepts" is invisible in practice.
    """

    def nullable_schema(self, schema):  # type: ignore[override]
        return self.generate_inner(schema["schema"])


def generate_meta_schema() -> dict[str, Any]:
    """Return the meta-schema as a Python ``dict``.

    The dict is safe to ``json.dump`` with ``indent=2`` to produce the
    artifact expected at ``spec/meta-schema.json``.
    """
    raw = MessageSchema.model_json_schema(
        schema_generator=CompactJsonSchema,
        mode="validation",
    )

    out: dict[str, Any] = dict(_META_SCHEMA_HEADER)
    for key, value in raw.items():
        if key in out:
            # Prefer our header values for $schema / $id / title / description.
            continue
        out[key] = value
    return out
