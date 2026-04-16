"""``schema`` command — operations on message schemas.

Currently provides a single subcommand, ``validate``, which checks that
one or more schema files conform to ``spec/meta-schema.json``. The
validation logic is exposed as a pure function (:func:`validate_schemas`)
for use from tests and other tooling; the CLI handler merely wraps it
with argument parsing, reporting, and exit-code translation.

Design: a schema that fails validation reports one structured error per
JSON-path location rather than a single top-level failure. This lets
reviewers fix all issues in one pass rather than one at a time.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass, field
from pathlib import Path

import jsonschema

from oigt_corpus_tools.paths import (
    find_repo_root,
    meta_schema_path,
    schemas_dir,
)


@dataclass
class ValidationError:
    """One validation failure inside a schema file."""

    location: str
    """Slash-joined JSON Pointer to the offending value, or ``<root>``."""

    message: str
    """Human-readable description of the violation."""


@dataclass
class ValidationResult:
    """Outcome of validating one schema file against the meta-schema."""

    path: Path
    errors: list[ValidationError] = field(default_factory=list)

    @property
    def ok(self) -> bool:
        return not self.errors


def validate_schemas(
    schema_paths: list[Path],
    meta_schema_file: Path,
) -> list[ValidationResult]:
    """Validate each schema file against the meta-schema.

    The ``$schema`` key, if present in a schema file, is stripped before
    validation — it is an editor hint pointing at the meta-schema, not
    part of the data the meta-schema describes.

    The meta-schema itself is verified to be a valid JSON Schema 2020-12
    document before validation proceeds; a malformed meta-schema raises
    ``jsonschema.exceptions.SchemaError``.
    """
    meta_schema = json.loads(meta_schema_file.read_text())
    jsonschema.Draft202012Validator.check_schema(meta_schema)
    validator = jsonschema.Draft202012Validator(meta_schema)

    results: list[ValidationResult] = []
    for path in schema_paths:
        data = json.loads(path.read_text())
        data = {k: v for k, v in data.items() if k != "$schema"}

        errors = [
            ValidationError(
                location="/".join(str(p) for p in err.absolute_path) or "<root>",
                message=err.message,
            )
            for err in validator.iter_errors(data)
        ]
        results.append(ValidationResult(path=path, errors=errors))

    return results


def register(parser: argparse.ArgumentParser) -> None:
    """Attach the ``schema`` command's subparsers and handlers."""
    subparsers = parser.add_subparsers(
        dest="subcommand",
        metavar="SUBCOMMAND",
        required=True,
    )

    validate = subparsers.add_parser(
        "validate",
        help="Validate schema files against spec/meta-schema.json.",
        description=(
            "Validate one or more message-schema JSON files against "
            "spec/meta-schema.json. With no arguments, validates every "
            "file under spec/schemas/."
        ),
    )
    validate.add_argument(
        "paths",
        nargs="*",
        type=Path,
        help="Schema files to validate. Defaults to spec/schemas/*.json.",
    )
    validate.set_defaults(handler=_cmd_validate)


def _cmd_validate(args: argparse.Namespace) -> int:
    try:
        repo_root = find_repo_root()
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    meta_schema_file = meta_schema_path(repo_root)
    if not meta_schema_file.is_file():
        print(f"error: meta-schema not found at {meta_schema_file}", file=sys.stderr)
        return 2

    if args.paths:
        paths = [p.resolve() for p in args.paths]
    else:
        paths = sorted(schemas_dir(repo_root).glob("*.json"))

    if not paths:
        print("no schema files to validate", file=sys.stderr)
        return 1

    results = validate_schemas(paths, meta_schema_file)

    failures = 0
    for result in results:
        try:
            rel = result.path.relative_to(repo_root)
        except ValueError:
            rel = result.path

        if result.ok:
            print(f"  OK  {rel}")
        else:
            failures += 1
            print(f"FAIL  {rel}")
            for err in result.errors:
                print(f"        at {err.location}: {err.message}")

    total = len(results)
    if failures:
        print(
            f"\n{failures} of {total} schema(s) failed validation.",
            file=sys.stderr,
        )
        return 1

    print(f"\nAll {total} schema(s) passed.")
    return 0
