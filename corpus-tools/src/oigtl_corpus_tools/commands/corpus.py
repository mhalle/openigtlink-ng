"""``corpus`` command — build and verify the conformance corpora.

Currently one subcommand:

- ``generate-negative`` — write ``spec/corpus/negative/**/*.bin`` and
  ``spec/corpus/negative/index.json`` from the deterministic generator
  in :mod:`oigtl_corpus_tools.negative_corpus`. With ``--check``,
  verify the on-disk files match and exit non-zero on drift.

The positive corpus (upstream fixtures) lives under
``fixtures export-json`` — same shape, different source.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from oigtl_corpus_tools.negative_corpus import build_corpus, index_payload
from oigtl_corpus_tools.paths import RepoRootNotFound, find_repo_root


def register(parser: argparse.ArgumentParser) -> None:
    subparsers = parser.add_subparsers(
        dest="subcommand", metavar="SUBCOMMAND", required=True,
    )
    _register_generate_negative(subparsers)


def _register_generate_negative(subparsers: argparse._SubParsersAction) -> None:
    p = subparsers.add_parser(
        "generate-negative",
        help="Write (or verify) the negative corpus under spec/corpus/negative/.",
        description=(
            "Generate every must-reject entry from the builder module and "
            "write them as .bin files plus a JSON index. With --check, "
            "compare against the on-disk files and exit non-zero on drift."
        ),
    )
    p.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="Destination directory (default: spec/corpus/negative/).",
    )
    p.add_argument(
        "--check",
        action="store_true",
        help="Do not write; compare. Exit 0 if in sync, 1 on drift.",
    )
    p.set_defaults(handler=_cmd_generate_negative)


def _cmd_generate_negative(args: argparse.Namespace) -> int:
    try:
        repo_root = find_repo_root()
    except RepoRootNotFound as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    out_dir = args.out_dir or repo_root / "spec" / "corpus" / "negative"
    entries = build_corpus()
    payload = index_payload(entries)
    index_text = json.dumps(payload, indent=2) + "\n"

    if args.check:
        drift = 0
        for e in entries:
            path = out_dir / e.path
            if not path.is_file():
                print(f"FAIL  missing {path}", file=sys.stderr)
                drift += 1
                continue
            if path.read_bytes() != e.data:
                print(f"FAIL  drift  {path}", file=sys.stderr)
                drift += 1
            else:
                print(f"  OK  {path}")
        idx_path = out_dir / "index.json"
        if not idx_path.is_file() or idx_path.read_text() != index_text:
            print(f"FAIL  drift  {idx_path}", file=sys.stderr)
            drift += 1
        else:
            print(f"  OK  {idx_path}")
        if drift:
            print(
                f"\n{drift} file(s) drifted. Run "
                "'oigtl-corpus corpus generate-negative' to regenerate.",
                file=sys.stderr,
            )
            return 1
        print(f"\nAll {len(entries) + 1} file(s) up to date.")
        return 0

    out_dir.mkdir(parents=True, exist_ok=True)
    for e in entries:
        path = out_dir / e.path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(e.data)
        print(f"  wrote {path.relative_to(repo_root)}  ({len(e.data)} bytes)")
    (out_dir / "index.json").write_text(index_text)
    print(f"  wrote {(out_dir / 'index.json').relative_to(repo_root)}")
    print(f"\n{len(entries)} entries + index.")
    return 0
