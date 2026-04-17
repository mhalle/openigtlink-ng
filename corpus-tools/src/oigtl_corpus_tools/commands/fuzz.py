"""``fuzz`` command — differential + structured fuzzing entry points.

Phase 2 subcommands:

- ``differential`` — drive the oracle-differential runner against a
  configurable subset of implementations (py-ref / cpp / ts).

Future phases will add ``corpus`` (replay the on-disk corpus),
``shrink`` (minimize a disagreeing input), and per-codec in-process
fuzzers wired via Atheris / libFuzzer / jsfuzz (those live in each
codec's own source tree, not here — this command just orchestrates
the cross-language comparison).
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from oigtl_corpus_tools.fuzz import runner
from oigtl_corpus_tools.paths import RepoRootNotFound, find_repo_root


def register(parser: argparse.ArgumentParser) -> None:
    subparsers = parser.add_subparsers(
        dest="subcommand", metavar="SUBCOMMAND", required=True,
    )
    _register_differential(subparsers)


def _register_differential(subparsers: argparse._SubParsersAction) -> None:
    p = subparsers.add_parser(
        "differential",
        help=(
            "Run the differential oracle fuzzer across py-ref / cpp / ts."
        ),
        description=(
            "Generate candidate wire-byte inputs and feed them through "
            "every selected oracle. Any semantic disagreement (ok "
            "mismatch, accepted-case field mismatch) is logged to "
            "--log-file. Exit non-zero on disagreement."
        ),
    )
    p.add_argument(
        "--iterations", "-n",
        type=int, default=10_000,
        help="Number of inputs to generate (default: 10000).",
    )
    p.add_argument(
        "--seed", "-s",
        type=int, default=42,
        help="PRNG seed for reproducibility (default: 42).",
    )
    p.add_argument(
        "--generator",
        action="append", dest="generators",
        choices=["random", "mutate", "structured"],
        help=(
            "Generator to use; may be given multiple times. "
            "Default: all three."
        ),
    )
    p.add_argument(
        "--oracle",
        action="append", dest="oracles",
        choices=["py-ref", "py", "py-noarray", "cpp", "ts"],
        help=(
            "Oracle to include; may be given multiple times. "
            "py-ref = reference dict codec (fastest, in-process). "
            "py = typed Python classes through numpy path. "
            "py-noarray = same but with OIGTL_NO_NUMPY=1 forcing "
            "the array.array fallback. cpp, ts: external CLIs. "
            "Default: py-ref only."
        ),
    )
    p.add_argument(
        "--cpp-binary",
        type=Path, default=None,
        help=(
            "Path to the built `oigtl_oracle_cli` binary. "
            "Required when --oracle cpp is used. "
            "Default: <repo>/core-cpp/build/oigtl_oracle_cli."
        ),
    )
    p.add_argument(
        "--ts-script",
        type=Path, default=None,
        help=(
            "Path to the built TS oracle_cli.js. "
            "Required when --oracle ts is used. "
            "Default: <repo>/core-ts/build-tests/src/oracle_cli.js."
        ),
    )
    p.add_argument(
        "--log-file",
        type=Path, default=None,
        help=(
            "Write one disagreement JSON per line to this path. "
            "Default: <repo>/security/disagreements/<seed>.jsonl."
        ),
    )
    p.add_argument(
        "--progress-every",
        type=int, default=1000,
        help="Emit a progress line every N iterations (0 to suppress).",
    )
    p.set_defaults(handler=_cmd_differential)


def _cmd_differential(args: argparse.Namespace) -> int:
    try:
        repo_root = find_repo_root()
    except RepoRootNotFound as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    generators = args.generators or ["random", "mutate", "structured"]
    oracles = args.oracles or ["py-ref"]

    cpp_binary = args.cpp_binary or (
        repo_root / "core-cpp" / "build" / "oigtl_oracle_cli"
    )
    ts_script = args.ts_script or (
        repo_root / "core-ts" / "build-tests" / "src" / "oracle_cli.js"
    )
    core_py_dir = repo_root / "core-py"
    log_file = args.log_file or (
        repo_root / "security" / "disagreements" / f"{args.seed}.jsonl"
    )

    try:
        result = runner.run(
            iterations=args.iterations,
            seed=args.seed,
            generators=generators,
            oracles=oracles,
            cpp_binary=cpp_binary if "cpp" in oracles else None,
            ts_script=ts_script if "ts" in oracles else None,
            core_py_dir=(
                core_py_dir
                if ("py" in oracles or "py-noarray" in oracles)
                else None
            ),
            disagreements_log=log_file,
            progress_every=args.progress_every,
        )
    except FileNotFoundError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    rate = (result.iterations / result.elapsed_sec
            if result.elapsed_sec > 0 else 0.0)
    print(f"\n{result.iterations} inputs in {result.elapsed_sec:.1f}s "
          f"({rate:.0f} it/s)")
    print(f"  oracles:    {', '.join(oracles)}")
    print(f"  generators: {', '.join(generators)}")
    print(f"  rejects: ",
          ", ".join(f"{k}={v}" for k, v in result.per_oracle_rejects.items()))
    print(f"  disagreements: {result.disagreements}")
    if result.disagreements > 0:
        print(f"  log: {log_file}")
        return 1
    return 0
