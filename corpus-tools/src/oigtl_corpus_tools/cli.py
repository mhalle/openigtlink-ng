"""Top-level CLI dispatcher.

Follows the command / subcommand pattern (``oigtl-corpus <command>
<subcommand> [options]``). Each command is a module in
``oigtl_corpus_tools.commands`` that exposes a ``register(parser)`` function
taking the argparse subparser for that command and attaching its own
subparsers and handlers.

Handlers are registered on argparse ``set_defaults(handler=...)`` and must
accept the parsed ``Namespace`` and return an integer exit code.
"""

from __future__ import annotations

import argparse
import sys
from collections.abc import Sequence

from oigtl_corpus_tools import __version__
from oigtl_corpus_tools.commands import codegen, corpus, fixtures, oracle, schema


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="oigtl-corpus",
        description="Tooling for the openigtlink-ng conformance corpus.",
    )
    parser.add_argument(
        "--version",
        action="version",
        version=f"%(prog)s {__version__}",
    )

    subparsers = parser.add_subparsers(
        dest="command",
        metavar="COMMAND",
        required=True,
    )

    schema_parser = subparsers.add_parser(
        "schema",
        help="Operations on message schemas under spec/schemas/.",
    )
    schema.register(schema_parser)

    codegen_parser = subparsers.add_parser(
        "codegen",
        help="Emit wire-codec source code from spec/schemas/.",
    )
    codegen.register(codegen_parser)

    oracle_parser = subparsers.add_parser(
        "oracle",
        help="Run the conformance oracle on wire bytes.",
    )
    oracle.register(oracle_parser)

    fixtures_parser = subparsers.add_parser(
        "fixtures",
        help="Export upstream conformance fixtures for non-Python consumers.",
    )
    fixtures.register(fixtures_parser)

    corpus_parser = subparsers.add_parser(
        "corpus",
        help="Generate or verify the negative (must-reject) corpus.",
    )
    corpus.register(corpus_parser)

    return parser


def main(argv: Sequence[str] | None = None) -> int:
    """Entry point for the ``oigtl-corpus`` console script.

    Returns the exit code. Console scripts that want to exit cleanly should
    wrap this call in ``sys.exit(main())``.
    """
    parser = _build_parser()
    args = parser.parse_args(argv)
    return args.handler(args)


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
