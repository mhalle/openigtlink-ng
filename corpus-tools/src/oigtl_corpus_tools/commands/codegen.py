"""``codegen`` command — emit wire-codec source for downstream languages.

Subcommands:

- ``cpp`` — emit the C++17 codec from ``spec/schemas/*.json`` into a
  pair of directories (one for ``.hpp``, one for ``.cpp``). With
  ``--check``, verify the on-disk files match what would be generated
  and exit non-zero on drift.

The codegen logic itself lives in :mod:`oigtl_corpus_tools.codegen`;
handlers here are thin argparse glue.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from oigtl_corpus_tools.codegen.cpp_emit import (
    RenderedMessage,
    render_message,
    render_register_all,
)
from oigtl_corpus_tools.paths import (
    RepoRootNotFound,
    find_repo_root,
    schemas_dir,
)


# ---------------------------------------------------------------------------
# CLI registration
# ---------------------------------------------------------------------------


def register(parser: argparse.ArgumentParser) -> None:
    subparsers = parser.add_subparsers(
        dest="subcommand",
        metavar="SUBCOMMAND",
        required=True,
    )
    _register_cpp(subparsers)


def _register_cpp(subparsers: argparse._SubParsersAction) -> None:
    cpp = subparsers.add_parser(
        "cpp",
        help="Emit C++17 wire codec from spec/schemas/*.json.",
        description=(
            "Render one .hpp + .cpp pair per message schema. With "
            "--check, exit non-zero if the on-disk output differs from "
            "what would be generated. Without --check, write files. "
            "Schemas whose field shape is not yet supported by the "
            "current codegen are skipped with a warning."
        ),
    )
    cpp.add_argument(
        "--include-dir",
        type=Path,
        default=Path("core-cpp/include/oigtl/messages"),
        help=(
            "Directory for generated .hpp files "
            "(default: core-cpp/include/oigtl/messages)."
        ),
    )
    cpp.add_argument(
        "--src-dir",
        type=Path,
        default=Path("core-cpp/src/messages"),
        help=(
            "Directory for generated .cpp files "
            "(default: core-cpp/src/messages)."
        ),
    )
    cpp.add_argument(
        "--type-id",
        action="append",
        default=None,
        help=(
            "Restrict generation to specific type_ids. May be given "
            "multiple times. Default: all schemas."
        ),
    )
    cpp.add_argument(
        "--check",
        action="store_true",
        help=(
            "Verify on-disk output matches what would be generated. "
            "Does not write. Non-zero exit on drift."
        ),
    )
    cpp.set_defaults(handler=_cmd_cpp)


# ---------------------------------------------------------------------------
# Handler
# ---------------------------------------------------------------------------


def _cmd_cpp(args: argparse.Namespace) -> int:
    try:
        repo_root = find_repo_root()
    except RepoRootNotFound as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    include_dir = (
        args.include_dir if args.include_dir.is_absolute()
        else repo_root / args.include_dir
    )
    src_dir = (
        args.src_dir if args.src_dir.is_absolute()
        else repo_root / args.src_dir
    )

    requested = set(args.type_id) if args.type_id else None
    schemas = _load_schemas(repo_root, requested)
    if not schemas:
        print("no schemas to render", file=sys.stderr)
        return 1

    rendered: list[RenderedMessage] = []
    skipped: list[tuple[str, str]] = []
    for type_id, schema in schemas:
        try:
            rendered.append(render_message(schema))
        except NotImplementedError as exc:
            skipped.append((type_id, str(exc)))

    # Render register_all only when the user asked for everything
    # (no --type-id filter); otherwise we'd emit a registry missing
    # the unselected types and break every consumer.
    registry = (
        render_register_all([r.type_id for r in rendered])
        if requested is None else None
    )

    if args.check:
        return _check_drift(rendered, include_dir, src_dir, registry, skipped)

    return _write_outputs(rendered, include_dir, src_dir, registry, skipped)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _load_schemas(
    repo_root: Path,
    requested_type_ids: set[str] | None,
) -> list[tuple[str, dict]]:
    """Load schemas under spec/schemas/, filtered by *requested_type_ids*.

    Returns a sorted list of ``(type_id, schema_dict)`` pairs. Schemas
    without a ``type_id`` (data files? meta artifacts?) are silently
    skipped — they're not message schemas.
    """
    out: list[tuple[str, dict]] = []
    for path in sorted(schemas_dir(repo_root).glob("*.json")):
        with open(path) as f:
            schema = json.load(f)
        type_id = schema.get("type_id")
        if not type_id:
            continue
        if requested_type_ids is not None and type_id not in requested_type_ids:
            continue
        out.append((type_id, schema))
    return out


def _write_outputs(
    rendered: list[RenderedMessage],
    include_dir: Path,
    src_dir: Path,
    registry,
    skipped: list[tuple[str, str]],
) -> int:
    include_dir.mkdir(parents=True, exist_ok=True)
    src_dir.mkdir(parents=True, exist_ok=True)
    for r in rendered:
        (include_dir / f"{r.header_basename}.hpp").write_text(r.hpp_text)
        (src_dir / f"{r.header_basename}.cpp").write_text(r.cpp_text)
        print(f"  wrote {r.type_id} → {r.header_basename}.hpp/.cpp")
    if registry is not None:
        (include_dir / "register_all.hpp").write_text(registry.hpp_text)
        (src_dir / "register_all.cpp").write_text(registry.cpp_text)
        print("  wrote register_all.hpp/.cpp")
    for type_id, reason in skipped:
        print(f"  SKIP {type_id}: {reason}", file=sys.stderr)
    print(
        f"\n{len(rendered)} rendered, {len(skipped)} skipped "
        "(unsupported field shape)."
    )
    return 0


def _check_drift(
    rendered: list[RenderedMessage],
    include_dir: Path,
    src_dir: Path,
    registry,
    skipped: list[tuple[str, str]],
) -> int:
    drift_count = 0
    for r in rendered:
        for path, expected in (
            (include_dir / f"{r.header_basename}.hpp", r.hpp_text),
            (src_dir / f"{r.header_basename}.cpp", r.cpp_text),
        ):
            if not path.is_file():
                print(f"FAIL  missing {path}", file=sys.stderr)
                drift_count += 1
                continue
            actual = path.read_text()
            if actual != expected:
                print(
                    f"FAIL  drift {path}", file=sys.stderr,
                )
                drift_count += 1
            else:
                print(f"  OK  {path}")
    if registry is not None:
        for path, expected in (
            (include_dir / "register_all.hpp", registry.hpp_text),
            (src_dir / "register_all.cpp", registry.cpp_text),
        ):
            if not path.is_file():
                print(f"FAIL  missing {path}", file=sys.stderr)
                drift_count += 1
                continue
            if path.read_text() != expected:
                print(f"FAIL  drift {path}", file=sys.stderr)
                drift_count += 1
            else:
                print(f"  OK  {path}")
    if skipped:
        print(
            f"\n{len(skipped)} skipped (unsupported field shape)",
            file=sys.stderr,
        )
    if drift_count:
        print(
            f"\n{drift_count} file(s) drifted. Run "
            "'uv run oigtl-corpus codegen cpp' to regenerate.",
            file=sys.stderr,
        )
        return 1
    print(f"\nAll {len(rendered) * 2} generated file(s) up to date.")
    return 0
