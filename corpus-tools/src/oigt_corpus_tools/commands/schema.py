"""``schema`` command — operations on message schemas.

Subcommands:

- ``validate`` — validate schema files against the Pydantic
  :class:`MessageSchema` model. The JSON Schema under
  ``spec/meta-schema.json`` exists for non-Python consumers; this
  command uses the Pydantic model directly as the authoritative
  validator.

- ``emit-meta`` — regenerate ``spec/meta-schema.json`` from the Pydantic
  models. With ``--check``, verify the committed file matches what would
  be generated and exit non-zero if it does not.

The validation logic is exposed as a pure function
(:func:`validate_schemas`) for use from tests and other tooling; CLI
handlers are thin wrappers that translate argparse namespaces, print
reports, and return exit codes.

Design: a schema that fails validation reports one structured error per
offending JSON-pointer location rather than a single top-level failure.
This lets reviewers fix all issues in one pass rather than one at a time.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from pydantic import ValidationError as PydanticValidationError

from oigt_corpus_tools.paths import (
    RepoRootNotFound,
    find_repo_root,
    meta_schema_path,
    schemas_dir,
)
from oigt_corpus_tools.schema import MessageSchema, generate_meta_schema


# ---------------------------------------------------------------------------
# Result types
# ---------------------------------------------------------------------------


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


# ---------------------------------------------------------------------------
# Pure validation logic
# ---------------------------------------------------------------------------


def validate_schemas(schema_paths: list[Path]) -> list[ValidationResult]:
    """Validate each schema file against the Pydantic ``MessageSchema`` model.

    The ``$schema`` key, if present in a schema file, is stripped before
    validation — it is an editor hint pointing at the meta-schema file,
    not part of the data the model describes.

    Returns a :class:`ValidationResult` per input path. Caller decides how
    to report.
    """
    results: list[ValidationResult] = []
    for path in schema_paths:
        data = json.loads(path.read_text())
        if isinstance(data, dict):
            data = {k: v for k, v in data.items() if k != "$schema"}

        errors: list[ValidationError] = []
        try:
            MessageSchema.model_validate(data)
        except PydanticValidationError as exc:
            errors.extend(_translate_error(e) for e in exc.errors())
        results.append(ValidationResult(path=path, errors=errors))
    return results


def _translate_error(err: dict[str, Any]) -> ValidationError:
    """Translate a Pydantic error dict into the :class:`ValidationError` shape.

    Pydantic's own ``msg`` values are fine in most cases. For two very
    common cases — missing required field, unexpected property — the
    error message is clearer when it names the offending key explicitly,
    so we reformat those.
    """
    loc = err.get("loc", ())
    location = "/".join(str(p) for p in loc) if loc else "<root>"
    etype = err.get("type", "")
    tail = str(loc[-1]) if loc else "<root>"

    if etype == "missing":
        message = f"'{tail}' is a required property"
    elif etype == "extra_forbidden":
        message = f"additional property '{tail}' is not allowed"
    else:
        message = err.get("msg", "validation error")

    return ValidationError(location=location, message=message)


# ---------------------------------------------------------------------------
# CLI registration
# ---------------------------------------------------------------------------


def register(parser: argparse.ArgumentParser) -> None:
    """Attach the ``schema`` command's subparsers and handlers."""
    subparsers = parser.add_subparsers(
        dest="subcommand",
        metavar="SUBCOMMAND",
        required=True,
    )

    _register_validate(subparsers)
    _register_emit_meta(subparsers)


def _register_validate(subparsers: argparse._SubParsersAction) -> None:
    validate = subparsers.add_parser(
        "validate",
        help="Validate schema files against the Pydantic MessageSchema model.",
        description=(
            "Validate one or more message-schema JSON files against the "
            "Pydantic MessageSchema model that is the source of truth for "
            "this project. With no arguments, validates every file under "
            "spec/schemas/."
        ),
    )
    validate.add_argument(
        "paths",
        nargs="*",
        type=Path,
        help="Schema files to validate. Defaults to spec/schemas/*.json.",
    )
    validate.set_defaults(handler=_cmd_validate)


def _register_emit_meta(subparsers: argparse._SubParsersAction) -> None:
    emit_meta = subparsers.add_parser(
        "emit-meta",
        help="Regenerate spec/meta-schema.json from the Pydantic models.",
        description=(
            "Generate spec/meta-schema.json from the Pydantic models in "
            "oigt_corpus_tools.schema (types / element / field / message). "
            "With --check, verify that the committed file matches what "
            "would be generated and exit non-zero if it does not. Without "
            "--check, write the file."
        ),
    )
    emit_meta.add_argument(
        "--check",
        action="store_true",
        help=(
            "Verify that spec/meta-schema.json is in sync with the Pydantic "
            "models. Does not write. Non-zero exit if out of sync."
        ),
    )
    emit_meta.set_defaults(handler=_cmd_emit_meta)


# ---------------------------------------------------------------------------
# Handlers
# ---------------------------------------------------------------------------


def _cmd_validate(args: argparse.Namespace) -> int:
    try:
        repo_root = find_repo_root()
    except RepoRootNotFound as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    if args.paths:
        paths = [p.resolve() for p in args.paths]
    else:
        paths = sorted(schemas_dir(repo_root).glob("*.json"))

    if not paths:
        print("no schema files to validate", file=sys.stderr)
        return 1

    results = validate_schemas(paths)

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


def _cmd_emit_meta(args: argparse.Namespace) -> int:
    try:
        repo_root = find_repo_root()
    except RepoRootNotFound as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    target = meta_schema_path(repo_root)
    generated_text = _render_meta_schema()

    try:
        rel = target.relative_to(repo_root)
    except ValueError:
        rel = target

    if args.check:
        if not target.is_file():
            print(f"error: {target} does not exist", file=sys.stderr)
            return 1
        current_text = target.read_text()
        if current_text == generated_text:
            print(f"  OK  {rel} is in sync with the Pydantic models.")
            return 0
        print(f"FAIL  {rel} is out of sync with the Pydantic models.", file=sys.stderr)
        print(
            "      Run 'uv run oigt-corpus schema emit-meta' to regenerate.",
            file=sys.stderr,
        )
        return 1

    target.write_text(generated_text)
    print(f"wrote {rel}")
    return 0


def _render_meta_schema() -> str:
    """Serialize the generated meta-schema to the exact string we write on disk.

    Exposed at module level so the ``--check`` path and any tests can
    compare against the same canonical output.
    """
    return json.dumps(generate_meta_schema(), indent=2) + "\n"
