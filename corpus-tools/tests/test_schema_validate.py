"""Tests for ``oigt_corpus_tools.commands.schema``.

Validation logic is exercised directly through the pure
:func:`validate_schemas` function rather than through the CLI, so tests
don't have to construct argparse namespaces or capture stdout. Two small
integration tests confirm that the real repository meta-schema and
exemplar schema load and validate together.
"""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from oigt_corpus_tools.commands.schema import validate_schemas
from oigt_corpus_tools.paths import find_repo_root, meta_schema_path, schemas_dir


# ---------------------------------------------------------------------------
# Real-repo integration: exemplar schemas validate against the real meta-schema
# ---------------------------------------------------------------------------


@pytest.fixture(scope="module")
def repo_root() -> Path:
    return find_repo_root()


@pytest.fixture(scope="module")
def real_meta_schema(repo_root: Path) -> Path:
    return meta_schema_path(repo_root)


def test_all_real_schemas_validate(repo_root: Path, real_meta_schema: Path) -> None:
    """Every schema under ``spec/schemas/`` must validate against the meta-schema."""
    paths = sorted(schemas_dir(repo_root).glob("*.json"))
    assert paths, "no schemas found to validate — did spec/schemas/ become empty?"

    results = validate_schemas(paths, real_meta_schema)
    failures = [r for r in results if not r.ok]

    assert not failures, "\n".join(
        f"{r.path.name}: " + "; ".join(f"{e.location}: {e.message}" for e in r.errors)
        for r in failures
    )


# ---------------------------------------------------------------------------
# Synthetic schemas: confirm the validator rejects the right things
# ---------------------------------------------------------------------------


def _write_schema(dir_: Path, name: str, contents: dict) -> Path:
    path = dir_ / name
    path.write_text(json.dumps(contents))
    return path


def test_well_formed_synthetic_schema_passes(
    tmp_path: Path, real_meta_schema: Path
) -> None:
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
    [result] = validate_schemas([path], real_meta_schema)
    assert result.ok, [f"{e.location}: {e.message}" for e in result.errors]


def test_missing_required_top_level_field_fails(
    tmp_path: Path, real_meta_schema: Path
) -> None:
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
    [result] = validate_schemas([path], real_meta_schema)

    assert not result.ok
    assert any("description" in err.message for err in result.errors), [
        err.message for err in result.errors
    ]


def test_field_missing_description_fails(
    tmp_path: Path, real_meta_schema: Path
) -> None:
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
    [result] = validate_schemas([path], real_meta_schema)

    assert not result.ok
    assert any("description" in err.message for err in result.errors)
    # The error should localize to the offending field, not the document root.
    assert any(err.location.startswith("fields/0") for err in result.errors), [
        err.location for err in result.errors
    ]


def test_invalid_field_name_pattern_fails(
    tmp_path: Path, real_meta_schema: Path
) -> None:
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
    [result] = validate_schemas([path], real_meta_schema)

    assert not result.ok
    assert any(err.location.startswith("fields/0/name") for err in result.errors), [
        err.location for err in result.errors
    ]


def test_unknown_top_level_key_fails(
    tmp_path: Path, real_meta_schema: Path
) -> None:
    """Top-level ``additionalProperties: false`` rejects unknown keys."""
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
    [result] = validate_schemas([path], real_meta_schema)

    assert not result.ok
    assert any(
        "totally_made_up_key" in err.message for err in result.errors
    ), [err.message for err in result.errors]
