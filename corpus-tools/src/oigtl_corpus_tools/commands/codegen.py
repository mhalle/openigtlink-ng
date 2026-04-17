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
from oigtl_corpus_tools.codegen import python_emit, ts_emit
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
    _register_cpp_compat(subparsers)
    _register_python(subparsers)
    _register_ts(subparsers)


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


def _register_cpp_compat(subparsers: argparse._SubParsersAction) -> None:
    c = subparsers.add_parser(
        "cpp-compat",
        help="Emit upstream-compat shim headers (igtl::FooMessage).",
        description=(
            "Generate the upstream-compatible shim facade headers under "
            "core-cpp/compat/include/igtl/. For header-only variants "
            "(GET_/STT_/STP_/RTS_) the output is fully functional; for "
            "data-carrying messages (STATUS, STRING, …) the output is a "
            "compile-only stub that a hand-written replacement can "
            "supersede. TRANSFORM is skipped to avoid clobbering the "
            "hand-written facade."
        ),
    )
    c.add_argument(
        "--include-dir",
        type=Path,
        default=Path("core-cpp/compat/include/igtl"),
        help=(
            "Output directory for generated igtl*.h files "
            "(default: core-cpp/compat/include/igtl)."
        ),
    )
    c.add_argument(
        "--check",
        action="store_true",
        help="Non-zero exit if on-disk output differs. Does not write.",
    )
    c.set_defaults(handler=_cmd_cpp_compat)


def _cmd_cpp_compat(args: argparse.Namespace) -> int:
    from oigtl_corpus_tools.codegen.cpp_compat import all_shim_files

    try:
        repo_root = find_repo_root()
    except RepoRootNotFound as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    include_dir = args.include_dir
    if not include_dir.is_absolute():
        include_dir = repo_root / include_dir

    # Skip type_ids whose facades are hand-written (already present
    # in core-cpp/compat/src/).
    hand_written = {"TRANSFORM", "GET_TRANS"}

    files = all_shim_files(skip_type_ids=hand_written)

    if args.check:
        drifted = []
        for sf in files:
            path = include_dir / sf.header_path
            if not path.is_file():
                drifted.append(path)
                continue
            if path.read_text() != sf.hpp_text:
                drifted.append(path)
        if drifted:
            print("error: drift in compat headers:", file=sys.stderr)
            for p in drifted:
                print(f"  {p.relative_to(repo_root)}", file=sys.stderr)
            return 1
        print(f"ok: {len(files)} compat headers in sync.")
        return 0

    include_dir.mkdir(parents=True, exist_ok=True)
    written = 0
    for sf in files:
        path = include_dir / sf.header_path
        path.write_text(sf.hpp_text)
        written += 1
    print(f"wrote {written} compat headers to "
          f"{include_dir.relative_to(repo_root)}")
    return 0


def _register_python(subparsers: argparse._SubParsersAction) -> None:
    py = subparsers.add_parser(
        "python",
        help="Emit typed Python wire codec from spec/schemas/*.json.",
        description=(
            "Render one Pydantic-model module per message schema "
            "into core-py/src/oigtl/messages/, plus an __init__.py "
            "that re-exports them and exposes a type_id → class "
            "REGISTRY. With --check, exit non-zero if the on-disk "
            "files differ from what would be generated."
        ),
    )
    py.add_argument(
        "--out-dir",
        type=Path,
        default=Path("core-py/src/oigtl/messages"),
        help=(
            "Directory for generated message modules + __init__.py "
            "(default: core-py/src/oigtl/messages)."
        ),
    )
    py.add_argument(
        "--type-id",
        action="append",
        default=None,
        help=(
            "Restrict generation to specific type_ids. May be given "
            "multiple times. Default: all schemas."
        ),
    )
    py.add_argument(
        "--check",
        action="store_true",
        help=(
            "Verify on-disk output matches what would be generated. "
            "Does not write. Non-zero exit on drift."
        ),
    )
    py.set_defaults(handler=_cmd_python)


def _register_ts(subparsers: argparse._SubParsersAction) -> None:
    ts = subparsers.add_parser(
        "ts",
        help="Emit typed TypeScript wire codec from spec/schemas/*.json.",
        description=(
            "Render one TS module per message schema into "
            "core-ts/src/messages/, plus an index.ts that re-exports "
            "them and registers them with the runtime dispatch "
            "registry. With --check, exit non-zero on drift."
        ),
    )
    ts.add_argument(
        "--out-dir",
        type=Path,
        default=Path("core-ts/src/messages"),
        help="Directory for generated message modules + index.ts.",
    )
    ts.add_argument(
        "--type-id",
        action="append",
        default=None,
        help="Restrict generation to specific type_ids.",
    )
    ts.add_argument(
        "--check",
        action="store_true",
        help="Verify on-disk output matches. No write. Non-zero on drift.",
    )
    ts.set_defaults(handler=_cmd_ts)


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


def _cmd_python(args: argparse.Namespace) -> int:
    try:
        repo_root = find_repo_root()
    except RepoRootNotFound as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    out_dir = (
        args.out_dir if args.out_dir.is_absolute()
        else repo_root / args.out_dir
    )
    requested = set(args.type_id) if args.type_id else None
    schemas = _load_schemas(repo_root, requested)
    if not schemas:
        print("no schemas to render", file=sys.stderr)
        return 1

    rendered: list[python_emit.RenderedPyMessage] = []
    skipped: list[tuple[str, str]] = []
    for type_id, schema in schemas:
        try:
            rendered.append(python_emit.render_message(schema))
        except NotImplementedError as exc:
            skipped.append((type_id, str(exc)))

    init = (
        python_emit.render_init([r.type_id for r in rendered])
        if requested is None else None
    )

    if args.check:
        drift = 0
        for r in rendered:
            path = out_dir / f"{r.module_name}.py"
            if not path.is_file():
                print(f"FAIL  missing {path}", file=sys.stderr)
                drift += 1
            elif path.read_text() != r.text:
                print(f"FAIL  drift {path}", file=sys.stderr)
                drift += 1
            else:
                print(f"  OK  {path}")
        if init is not None:
            path = out_dir / "__init__.py"
            if not path.is_file() or path.read_text() != init.text:
                print(f"FAIL  drift {path}", file=sys.stderr)
                drift += 1
            else:
                print(f"  OK  {path}")
        if skipped:
            print(f"\n{len(skipped)} skipped (unsupported field shape)",
                  file=sys.stderr)
        if drift:
            print(
                f"\n{drift} file(s) drifted. Run "
                "'uv run oigtl-corpus codegen python' to regenerate.",
                file=sys.stderr,
            )
            return 1
        print(f"\nAll generated file(s) up to date.")
        return 0

    out_dir.mkdir(parents=True, exist_ok=True)
    for r in rendered:
        (out_dir / f"{r.module_name}.py").write_text(r.text)
        print(f"  wrote {r.type_id} → {r.module_name}.py")
    if init is not None:
        (out_dir / "__init__.py").write_text(init.text)
        print("  wrote __init__.py")
    for tid, reason in skipped:
        print(f"  SKIP {tid}: {reason}", file=sys.stderr)
    print(f"\n{len(rendered)} rendered, {len(skipped)} skipped.")
    return 0


def _cmd_ts(args: argparse.Namespace) -> int:
    try:
        repo_root = find_repo_root()
    except RepoRootNotFound as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    out_dir = (
        args.out_dir if args.out_dir.is_absolute()
        else repo_root / args.out_dir
    )
    requested = set(args.type_id) if args.type_id else None
    schemas = _load_schemas(repo_root, requested)
    if not schemas:
        print("no schemas to render", file=sys.stderr)
        return 1

    rendered: list[ts_emit.RenderedTsMessage] = []
    skipped: list[tuple[str, str]] = []
    for type_id, schema in schemas:
        try:
            rendered.append(ts_emit.render_message(schema))
        except NotImplementedError as exc:
            skipped.append((type_id, str(exc)))

    index = (
        ts_emit.render_index([r.type_id for r in rendered])
        if requested is None else None
    )

    if args.check:
        drift = 0
        for r in rendered:
            path = out_dir / f"{r.module_name}.ts"
            if not path.is_file():
                print(f"FAIL  missing {path}", file=sys.stderr)
                drift += 1
            elif path.read_text() != r.text:
                print(f"FAIL  drift {path}", file=sys.stderr)
                drift += 1
            else:
                print(f"  OK  {path}")
        if index is not None:
            path = out_dir / "index.ts"
            if not path.is_file() or path.read_text() != index.text:
                print(f"FAIL  drift {path}", file=sys.stderr)
                drift += 1
            else:
                print(f"  OK  {path}")
        if skipped:
            print(
                f"\n{len(skipped)} skipped (unsupported field shape)",
                file=sys.stderr,
            )
        if drift:
            print(
                f"\n{drift} file(s) drifted. Run "
                "'uv run oigtl-corpus codegen ts' to regenerate.",
                file=sys.stderr,
            )
            return 1
        print(f"\nAll generated file(s) up to date.")
        return 0

    out_dir.mkdir(parents=True, exist_ok=True)
    for r in rendered:
        (out_dir / f"{r.module_name}.ts").write_text(r.text)
        print(f"  wrote {r.type_id} → {r.module_name}.ts")
    if index is not None:
        (out_dir / "index.ts").write_text(index.text)
        print("  wrote index.ts")
    for tid, reason in skipped:
        print(f"  SKIP {tid}: {reason}", file=sys.stderr)
    print(f"\n{len(rendered)} rendered, {len(skipped)} skipped.")
    return 0


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
