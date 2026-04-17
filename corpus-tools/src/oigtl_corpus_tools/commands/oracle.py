"""``oracle`` command — run the conformance oracle on wire bytes.

Subcommands:

- ``verify`` — feed bytes (from a named fixture, a hex literal, or
  stdin) through the oracle and emit a structured JSON report. The
  intended consumer is a cross-language parity test in
  ``core-cpp/tests/`` that asks the C++ oracle and the Python oracle
  to verify the same bytes and confirms they agree.

The output schema is small and stable:

    { "ok": bool,
      "type_id": str,
      "device_name": str,
      "version": int,
      "body_size": int,
      "ext_header_size": int|null,
      "metadata_count": int,
      "round_trip_ok": bool,
      "error": str }

When ``ok`` is false, the other fields may be partially populated
up to the point of failure (matches the Python oracle's behaviour).
"""

from __future__ import annotations

import argparse
import json
import sys
from typing import Any

from oigtl_corpus_tools.codec.oracle import OracleResult, verify_wire_bytes
from oigtl_corpus_tools.codec.test_vectors import UPSTREAM_VECTORS


# ---------------------------------------------------------------------------
# Result serialization
# ---------------------------------------------------------------------------


def _result_to_dict(result: OracleResult) -> dict[str, Any]:
    """Flatten an OracleResult into the stable cross-language report shape."""
    ext_size = (
        result.extended_header.get("ext_header_size")
        if result.extended_header is not None
        else None
    )
    return {
        "ok": result.ok,
        "type_id": result.type_id,
        "device_name": result.device_name,
        "version": int(result.header.get("version", 0)) if result.header else 0,
        "body_size": (
            int(result.header.get("body_size", 0)) if result.header else 0
        ),
        "ext_header_size": ext_size,
        "metadata_count": len(result.metadata),
        "round_trip_ok": result.round_trip_ok,
        "error": result.error,
    }


# ---------------------------------------------------------------------------
# CLI registration
# ---------------------------------------------------------------------------


def register(parser: argparse.ArgumentParser) -> None:
    subparsers = parser.add_subparsers(
        dest="subcommand", metavar="SUBCOMMAND", required=True,
    )
    _register_verify(subparsers)
    _register_list_fixtures(subparsers)


def _register_verify(subparsers: argparse._SubParsersAction) -> None:
    verify = subparsers.add_parser(
        "verify",
        help="Run the conformance oracle on wire bytes.",
        description=(
            "Feed bytes through the codec/oracle and emit a JSON report. "
            "Intended for the cross-language C++↔Python parity test, but "
            "also useful for ad-hoc inspection of suspect captures."
        ),
    )
    src = verify.add_mutually_exclusive_group(required=True)
    src.add_argument(
        "--fixture",
        type=str,
        help=(
            "Name of an upstream test fixture (run "
            "'oigtl-corpus oracle list-fixtures' to enumerate)."
        ),
    )
    src.add_argument(
        "--hex",
        type=str,
        help="Hex string of wire bytes (whitespace ignored).",
    )
    src.add_argument(
        "--stdin",
        action="store_true",
        help="Read raw wire bytes from stdin.",
    )
    verify.add_argument(
        "--no-crc",
        action="store_true",
        help="Skip CRC verification (still parses framing and dispatches).",
    )
    verify.set_defaults(handler=_cmd_verify)


def _register_list_fixtures(subparsers: argparse._SubParsersAction) -> None:
    lst = subparsers.add_parser(
        "list-fixtures",
        help="Print one upstream fixture name per line, sorted.",
    )
    lst.set_defaults(handler=_cmd_list_fixtures)


# ---------------------------------------------------------------------------
# Handlers
# ---------------------------------------------------------------------------


def _cmd_verify(args: argparse.Namespace) -> int:
    if args.fixture is not None:
        if args.fixture not in UPSTREAM_VECTORS:
            print(
                f"error: unknown fixture {args.fixture!r}. Run "
                "'oigtl-corpus oracle list-fixtures' to enumerate.",
                file=sys.stderr,
            )
            return 2
        data = UPSTREAM_VECTORS[args.fixture]
    elif args.hex is not None:
        try:
            data = bytes.fromhex("".join(args.hex.split()))
        except ValueError as exc:
            print(f"error: invalid hex: {exc}", file=sys.stderr)
            return 2
    else:  # --stdin
        data = sys.stdin.buffer.read()

    result = verify_wire_bytes(data, check_crc=not args.no_crc)
    print(json.dumps(_result_to_dict(result), indent=2))
    return 0 if result.ok else 1


def _cmd_list_fixtures(_args: argparse.Namespace) -> int:
    for name in sorted(UPSTREAM_VECTORS.keys()):
        print(name)
    return 0
