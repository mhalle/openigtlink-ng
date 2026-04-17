"""``fixtures`` command — export upstream conformance fixtures.

The upstream reference C library ships its test vectors as C headers
under ``reference-libs/``. The Python extractor in
``oigtl_corpus_tools.codec.test_vectors`` parses those headers and
reassembles multi-region fixtures (BIND, POLYDATA, NDARRAY) into
complete wire messages exposed via ``UPSTREAM_VECTORS``.

This command serializes that dictionary as a JSON file so other
implementations (TypeScript, Rust, …) can consume the same fixtures
without having to port the header extractor.

Subcommands:

- ``export-json`` — write/verify ``spec/corpus/upstream-fixtures.json``.

Output shape (stable, versioned by ``format_version``)::

    {
      "format_version": 1,
      "count": 24,
      "fixtures": {
        "transform": {
          "type_id": "TRANSFORM",
          "version": 1,
          "device_name": "DeviceName",
          "body_size": 48,
          "wire_hex": "000154524e..."
        },
        ...
      }
    }

The ``wire_hex`` field is the complete wire message — 58-byte
header + body — as lowercase hex without whitespace.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from oigtl_corpus_tools.codec.header import unpack_header
from oigtl_corpus_tools.codec.test_vectors import UPSTREAM_VECTORS
from oigtl_corpus_tools.paths import find_repo_root


FIXTURES_FORMAT_VERSION = 1


def _default_output_path() -> Path:
    return find_repo_root() / "spec" / "corpus" / "upstream-fixtures.json"


def _build_payload() -> dict[str, Any]:
    """Serialize UPSTREAM_VECTORS into the on-disk JSON shape."""
    fixtures: dict[str, Any] = {}
    for name in sorted(UPSTREAM_VECTORS.keys()):
        data = UPSTREAM_VECTORS[name]
        try:
            header = unpack_header(data)
        except Exception:  # pragma: no cover - every fixture has a valid header
            header = {}
        fixtures[name] = {
            "type_id": header.get("type", ""),
            "version": int(header.get("version", 0)),
            "device_name": header.get("device_name", ""),
            "body_size": int(header.get("body_size", 0)),
            "wire_hex": data.hex(),
        }
    return {
        "format_version": FIXTURES_FORMAT_VERSION,
        "count": len(fixtures),
        "fixtures": fixtures,
    }


def _serialize(payload: dict[str, Any]) -> str:
    # Pretty-printed, stable ordering, trailing newline. Keeping the
    # file diff-friendly matters because it is committed to the repo
    # and drift-checked in CI.
    return json.dumps(payload, indent=2, sort_keys=False) + "\n"


# ---------------------------------------------------------------------------
# CLI registration
# ---------------------------------------------------------------------------


def register(parser: argparse.ArgumentParser) -> None:
    subparsers = parser.add_subparsers(
        dest="subcommand", metavar="SUBCOMMAND", required=True,
    )
    _register_export_json(subparsers)


def _register_export_json(subparsers: argparse._SubParsersAction) -> None:
    p = subparsers.add_parser(
        "export-json",
        help="Write the upstream fixtures to a JSON file.",
        description=(
            "Serialize every fixture in UPSTREAM_VECTORS to a JSON file "
            "consumed by non-Python implementations (core-ts, etc.). "
            "Without --output, writes to spec/corpus/upstream-fixtures.json. "
            "With --check, compares against the existing file and exits "
            "nonzero on drift."
        ),
    )
    p.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Destination path (default: spec/corpus/upstream-fixtures.json).",
    )
    p.add_argument(
        "--stdout",
        action="store_true",
        help="Write to stdout instead of a file (ignores --output).",
    )
    p.add_argument(
        "--check",
        action="store_true",
        help=(
            "Don't write; compare the current file to what would be "
            "emitted. Exit 0 if identical, 1 on drift."
        ),
    )
    p.set_defaults(handler=_cmd_export_json)


# ---------------------------------------------------------------------------
# Handlers
# ---------------------------------------------------------------------------


def _cmd_export_json(args: argparse.Namespace) -> int:
    payload = _build_payload()
    text = _serialize(payload)

    if args.stdout:
        sys.stdout.write(text)
        return 0

    output: Path = args.output or _default_output_path()

    if args.check:
        if not output.exists():
            print(
                f"error: {output} does not exist; run without --check "
                "to create it.",
                file=sys.stderr,
            )
            return 1
        existing = output.read_text()
        if existing != text:
            print(
                f"drift: {output} is out of date. "
                "Run 'oigtl-corpus fixtures export-json' to regenerate.",
                file=sys.stderr,
            )
            return 1
        print(f"OK  {output}")
        return 0

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(text)
    print(
        f"wrote {output}  ({payload['count']} fixtures, "
        f"{len(text):,} bytes)"
    )
    return 0
