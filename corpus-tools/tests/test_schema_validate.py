"""Tests for ``oigtl_corpus_tools.commands.schema``.

Validation logic is exercised directly through the pure
:func:`validate_schemas` function rather than through the CLI, so tests
don't have to construct argparse namespaces or capture stdout. Two
integration tests confirm that (a) the real repository's exemplar
schemas all validate, and (b) the committed ``spec/meta-schema.json`` is
in sync with the Pydantic source of truth.
"""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from oigtl_corpus_tools.commands.schema import (
    _render_meta_schema,
    validate_schemas,
)
from oigtl_corpus_tools.paths import (
    find_repo_root,
    meta_schema_path,
    schemas_dir,
)


# ---------------------------------------------------------------------------
# Real-repo integration
# ---------------------------------------------------------------------------


@pytest.fixture(scope="module")
def repo_root() -> Path:
    return find_repo_root()


def test_all_real_schemas_validate(repo_root: Path) -> None:
    """Every schema under ``spec/schemas/`` must validate against the Pydantic model."""
    paths = sorted(schemas_dir(repo_root).glob("*.json"))
    assert paths, "no schemas found to validate — did spec/schemas/ become empty?"

    results = validate_schemas(paths)
    failures = [r for r in results if not r.ok]

    assert not failures, "\n".join(
        f"{r.path.name}: " + "; ".join(f"{e.location}: {e.message}" for e in r.errors)
        for r in failures
    )


def test_meta_schema_on_disk_in_sync_with_models(repo_root: Path) -> None:
    """The committed ``spec/meta-schema.json`` must match what the Pydantic models emit.

    Equivalent to running ``oigtl-corpus schema emit-meta --check``; this
    test form gives a useful diff when pytest's ``-vv`` mode is used.
    """
    expected = _render_meta_schema()
    actual = meta_schema_path(repo_root).read_text()

    assert actual == expected, (
        "spec/meta-schema.json is out of sync with the Pydantic models. "
        "Run 'uv run oigtl-corpus schema emit-meta' to regenerate."
    )


# ---------------------------------------------------------------------------
# Synthetic schemas: confirm the validator rejects the right things
# ---------------------------------------------------------------------------


def _write_schema(dir_: Path, name: str, contents: dict) -> Path:
    path = dir_ / name
    path.write_text(json.dumps(contents))
    return path


def test_well_formed_synthetic_schema_passes(tmp_path: Path) -> None:
    """A minimally-valid schema with all required fields must pass."""
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "A synthetic message type used for testing the validator.",
        "body_size": 0,
        "fields": [],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])
    assert result.ok, [f"{e.location}: {e.message}" for e in result.errors]


def test_missing_required_top_level_field_fails(tmp_path: Path) -> None:
    """Omitting a required top-level field must produce a root-level error."""
    schema = {
        # "description" deliberately omitted
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "body_size": 0,
        "fields": [],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])

    assert not result.ok
    assert any("description" in err.message for err in result.errors), [
        err.message for err in result.errors
    ]


def test_field_missing_description_fails(tmp_path: Path) -> None:
    """Every field in the schema is required to have a non-empty description."""
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test.",
        "body_size": 4,
        "fields": [
            {
                "name": "value",
                "type": "uint32",
                # "description" deliberately omitted
            }
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])

    assert not result.ok
    assert any("description" in err.message for err in result.errors)
    # The error should localize to the offending field, not the document root.
    assert any(err.location.startswith("fields/0") for err in result.errors), [
        err.location for err in result.errors
    ]


def test_invalid_field_name_pattern_fails(tmp_path: Path) -> None:
    """Field names must match ``^[a-z][a-z0-9_]*$`` — CamelCase is rejected."""
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test.",
        "body_size": 4,
        "fields": [
            {
                "name": "BadName",
                "type": "uint32",
                "description": "A field with a disallowed name.",
            }
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])

    assert not result.ok
    assert any(err.location.startswith("fields/0/name") for err in result.errors), [
        err.location for err in result.errors
    ]


def test_unknown_top_level_key_fails(tmp_path: Path) -> None:
    """Top-level ``extra='forbid'`` rejects unknown keys."""
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test.",
        "body_size": 0,
        "fields": [],
        "totally_made_up_key": "oops",
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])

    assert not result.ok
    assert any(
        "totally_made_up_key" in err.message for err in result.errors
    ), [err.message for err in result.errors]


# ---------------------------------------------------------------------------
# Cross-field validators (new with Pydantic migration)
# ---------------------------------------------------------------------------


def test_trailing_string_must_be_last_field(tmp_path: Path) -> None:
    """A trailing_string placed before another field must be rejected."""
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test.",
        "body_size": "variable",
        "fields": [
            {
                "name": "early_trailing",
                "type": "trailing_string",
                "encoding": "ascii",
                "description": "Disallowed: not the last field.",
            },
            {
                "name": "after",
                "type": "uint32",
                "description": "This field follows a trailing_string, which is wrong.",
            },
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])

    assert not result.ok
    assert any(
        "trailing_string" in err.message and "last" in err.message
        for err in result.errors
    ), [err.message for err in result.errors]


def test_fixed_string_requires_size_bytes(tmp_path: Path) -> None:
    """A fixed_string field without size_bytes must be rejected."""
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test.",
        "body_size": "variable",
        "fields": [
            {
                "name": "name",
                "type": "fixed_string",
                # "size_bytes" deliberately omitted
                "description": "Disallowed: fixed_string without size_bytes.",
            }
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])

    assert not result.ok
    assert any(
        "size_bytes" in err.message for err in result.errors
    ), [err.message for err in result.errors]


def test_length_prefixed_string_requires_length_prefix_type(tmp_path: Path) -> None:
    """A length_prefixed_string field without length_prefix_type must be rejected."""
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test.",
        "body_size": "variable",
        "fields": [
            {
                "name": "message",
                "type": "length_prefixed_string",
                # "length_prefix_type" deliberately omitted
                "encoding": "utf-8",
                "description": "Disallowed: no length_prefix_type.",
            }
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])

    assert not result.ok
    assert any(
        "length_prefix_type" in err.message for err in result.errors
    ), [err.message for err in result.errors]


def test_array_requires_count_or_count_from(tmp_path: Path) -> None:
    """An array field with neither `count` nor `count_from` must be rejected."""
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test.",
        "body_size": "variable",
        "fields": [
            {
                "name": "data",
                "type": "array",
                "element_type": "uint32",
                # neither count nor count_from — invalid
                "description": "Disallowed: array without count or count_from.",
            }
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])

    assert not result.ok
    assert any(
        "count" in err.message and "count_from" in err.message
        for err in result.errors
    ), [err.message for err in result.errors]


def test_array_cannot_have_both_count_and_count_from(tmp_path: Path) -> None:
    """An array field with both `count` and `count_from` set must be rejected."""
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test.",
        "body_size": "variable",
        "fields": [
            {
                "name": "data",
                "type": "array",
                "element_type": "uint32",
                "count": 10,
                "count_from": "remaining",
                "description": "Disallowed: array with both count and count_from.",
            }
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])

    assert not result.ok
    assert any(
        "both" in err.message for err in result.errors
    ), [err.message for err in result.errors]


def test_array_with_count_from_remaining_passes(tmp_path: Path) -> None:
    """An array with count_from='remaining' and no explicit count is valid."""
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test exercising count_from=remaining.",
        "body_size": "variable",
        "fields": [
            {
                "name": "data",
                "type": "array",
                "element_type": "uint32",
                "count_from": "remaining",
                "description": (
                    "Variable-length array; element count is body_size / 4."
                ),
            }
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])
    assert result.ok, [f"{e.location}: {e.message}" for e in result.errors]


def test_count_from_rejects_unknown_source(tmp_path: Path) -> None:
    """Any count_from value other than a defined CountSource variant is rejected."""
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test.",
        "body_size": "variable",
        "fields": [
            {
                "name": "data",
                "type": "array",
                "element_type": "uint32",
                "count_from": "body_remainder",  # not a valid CountSource value
                "description": "Disallowed: unknown count_from value.",
            }
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])

    assert not result.ok
    assert any(
        err.location.endswith("count_from") for err in result.errors
    ), [err.location for err in result.errors]


# ---------------------------------------------------------------------------
# ElementDescriptor: inline element specifications
# ---------------------------------------------------------------------------


def test_array_with_inline_fixed_string_element_passes(tmp_path: Path) -> None:
    """An array whose element_type is an inline ElementDescriptor is valid.

    This is the shape used by CAPABILITY: an array of 12-byte
    null-padded ASCII strings.
    """
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test of inline fixed_string element.",
        "body_size": "variable",
        "fields": [
            {
                "name": "types",
                "type": "array",
                "element_type": {
                    "type": "fixed_string",
                    "size_bytes": 12,
                    "encoding": "ascii",
                    "null_padded": True,
                },
                "count_from": "remaining",
                "description": "Array of 12-byte type-id strings.",
            }
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])
    assert result.ok, [f"{e.location}: {e.message}" for e in result.errors]


def test_inline_fixed_string_element_requires_size_bytes(tmp_path: Path) -> None:
    """An inline fixed_string element without size_bytes must be rejected."""
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test.",
        "body_size": "variable",
        "fields": [
            {
                "name": "types",
                "type": "array",
                "element_type": {
                    "type": "fixed_string",
                    # size_bytes missing — element descriptor must reject
                    "encoding": "ascii",
                },
                "count_from": "remaining",
                "description": "Disallowed: fixed_string element without size_bytes.",
            }
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])

    assert not result.ok
    assert any(
        "size_bytes" in err.message for err in result.errors
    ), [err.message for err in result.errors]


def test_trailing_string_cannot_be_array_element(tmp_path: Path) -> None:
    """trailing_string makes no sense as an element; must be rejected."""
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test.",
        "body_size": "variable",
        "fields": [
            {
                "name": "items",
                "type": "array",
                "element_type": {
                    "type": "trailing_string",
                    "encoding": "ascii",
                },
                "count_from": "remaining",
                "description": "Disallowed: trailing_string element.",
            }
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])

    assert not result.ok
    assert any(
        "trailing_string" in err.message for err in result.errors
    ), [err.message for err in result.errors]


def test_count_from_remaining_with_variable_element_rejected(tmp_path: Path) -> None:
    """count_from=remaining is invalid when element size is not static.

    A struct reference (UpperIdentifier) is the canonical case: struct
    layouts are not yet resolvable at schema-validation time, so we
    cannot compute body_size / element_size.
    """
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test.",
        "body_size": "variable",
        "fields": [
            {
                "name": "items",
                "type": "struct_array",
                "element_type": "SOME_STRUCT",
                "count_from": "remaining",
                "description": "Disallowed until struct sizes are resolvable.",
            }
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])

    assert not result.ok
    assert any(
        "statically-known" in err.message for err in result.errors
    ), [err.message for err in result.errors]


def test_count_from_remaining_with_primitive_element_passes(tmp_path: Path) -> None:
    """count_from=remaining is valid when element_type is a primitive."""
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test.",
        "body_size": "variable",
        "fields": [
            {
                "name": "data",
                "type": "array",
                "element_type": "float32",
                "count_from": "remaining",
                "description": "Variable-length float32 array.",
            }
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])
    assert result.ok, [f"{e.location}: {e.message}" for e in result.errors]


# ---------------------------------------------------------------------------
# Struct element type (nested FieldSchema list inside ElementDescriptor)
# ---------------------------------------------------------------------------


def test_struct_element_with_mixed_fields_passes(tmp_path: Path) -> None:
    """A struct element with primitive, fixed_string, and array sub-fields
    is valid when all sub-fields have statically-known sizes.

    This is the shape used by POINT, TDATA, etc.
    """
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test exercising struct-element arrays.",
        "body_size": "variable",
        "fields": [
            {
                "name": "entries",
                "type": "array",
                "element_type": {
                    "type": "struct",
                    "fields": [
                        {
                            "name": "name",
                            "type": "fixed_string",
                            "size_bytes": 16,
                            "encoding": "ascii",
                            "null_padded": True,
                            "description": "Entry name.",
                        },
                        {
                            "name": "kind",
                            "type": "uint8",
                            "description": "Entry kind.",
                        },
                        {
                            "name": "reserved",
                            "type": "uint8",
                            "description": "Reserved.",
                        },
                        {
                            "name": "vector",
                            "type": "array",
                            "element_type": "float32",
                            "count": 3,
                            "description": "3-vector.",
                        },
                    ],
                },
                "count_from": "remaining",
                "description": "Array of 30-byte struct entries.",
            }
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])
    assert result.ok, [f"{e.location}: {e.message}" for e in result.errors]


def test_struct_element_requires_fields(tmp_path: Path) -> None:
    """A struct-typed element with no `fields` list must be rejected."""
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test.",
        "body_size": "variable",
        "fields": [
            {
                "name": "entries",
                "type": "array",
                "element_type": {
                    "type": "struct",
                    # fields: deliberately omitted
                },
                "count_from": "remaining",
                "description": "Disallowed: struct element without fields.",
            }
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])

    assert not result.ok
    assert any(
        "struct" in err.message and "fields" in err.message
        for err in result.errors
    ), [err.message for err in result.errors]


def test_fields_on_non_struct_element_rejected(tmp_path: Path) -> None:
    """An element with `fields` set but a non-struct type must be rejected."""
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test.",
        "body_size": "variable",
        "fields": [
            {
                "name": "data",
                "type": "array",
                "element_type": {
                    "type": "uint32",
                    "fields": [
                        {
                            "name": "a",
                            "type": "uint32",
                            "description": "Disallowed: primitive can't have fields.",
                        }
                    ],
                },
                "count_from": "remaining",
                "description": "Disallowed: fields on a primitive element.",
            }
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])

    assert not result.ok
    assert any(
        "fields" in err.message and "struct" in err.message
        for err in result.errors
    ), [err.message for err in result.errors]


def test_struct_element_with_variable_subfield_blocks_count_from_remaining(
    tmp_path: Path,
) -> None:
    """count_from=remaining requires a statically-sized element.

    If a struct element contains a variable-size sub-field (e.g. a
    length_prefixed_string), the struct's total size is not statically
    known, so count_from=remaining must be rejected.
    """
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test.",
        "body_size": "variable",
        "fields": [
            {
                "name": "entries",
                "type": "array",
                "element_type": {
                    "type": "struct",
                    "fields": [
                        {
                            "name": "tag",
                            "type": "uint32",
                            "description": "Fixed tag.",
                        },
                        {
                            "name": "payload",
                            "type": "length_prefixed_string",
                            "length_prefix_type": "uint16",
                            "encoding": "utf-8",
                            "description": "Variable-size payload.",
                        },
                    ],
                },
                "count_from": "remaining",
                "description": (
                    "Disallowed: struct contains a variable-size sub-field."
                ),
            }
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])

    assert not result.ok
    assert any(
        "statically-known" in err.message for err in result.errors
    ), [err.message for err in result.errors]


def test_struct_element_with_explicit_count_passes(tmp_path: Path) -> None:
    """With an explicit `count`, a struct element is fine even if the
    struct happens to be fixed-size — demonstrates that struct elements
    work in both count-modes."""
    schema = {
        "message_type": "TEST",
        "type_id": "TEST",
        "introduced_in": "v1",
        "description": "Synthetic test.",
        "body_size": 24,
        "fields": [
            {
                "name": "entries",
                "type": "array",
                "element_type": {
                    "type": "struct",
                    "fields": [
                        {
                            "name": "id",
                            "type": "uint32",
                            "description": "Entry ID.",
                        },
                        {
                            "name": "value",
                            "type": "float64",
                            "description": "Value.",
                        },
                    ],
                },
                "count": 2,
                "description": "Exactly 2 entries of 12 bytes each = 24 bytes.",
            }
        ],
    }
    path = _write_schema(tmp_path, "test.json", schema)
    [result] = validate_schemas([path])
    assert result.ok, [f"{e.location}: {e.message}" for e in result.errors]
