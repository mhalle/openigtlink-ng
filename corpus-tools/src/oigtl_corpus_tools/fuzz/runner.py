"""Differential oracle runner.

Spawns one persistent oracle subprocess per requested implementation
(``py-ref`` runs in-process, ``cpp`` and ``ts`` are external CLIs)
and feeds each candidate input to every one of them. After every
iteration, the reports are cross-checked: any *semantic* mismatch
(different ``ok`` values, or different accepted-case fields) is a
bug and gets recorded as a disagreement.

Output:

- Writes human-readable progress to stderr.
- On any disagreement, writes a JSON line to the disagreements
  log: the candidate input (hex), all oracle reports, and which
  fields disagreed. The file becomes a corpus of real cross-codec
  bugs waiting to be triaged.

Exit codes:
- 0: no disagreements across the full run
- 1: disagreements found (details in the log file)
- 2: setup failure (oracle subprocess couldn't start)
"""

from __future__ import annotations

import dataclasses
import json
import random
import subprocess
import sys
import time
from pathlib import Path
from typing import IO, Any

from oigtl_corpus_tools.codec.oracle import verify_wire_bytes as py_verify
from oigtl_corpus_tools.commands.oracle import _result_to_dict
from oigtl_corpus_tools.fuzz.generators import GENERATORS, iter_candidates


# Fields that MUST agree across accepting oracles. The ``error`` field
# is excluded — different implementations worded errors differently
# and we already enforce a common error_class vocabulary in the
# negative corpus. The round_trip_ok boolean is included because a
# divergence there is a real round-trip-symmetry bug in one codec.
_SEMANTIC_FIELDS: tuple[str, ...] = (
    "ok",
    "type_id",
    "device_name",
    "version",
    "body_size",
    "ext_header_size",
    "metadata_count",
    "round_trip_ok",
)


@dataclasses.dataclass
class OracleSubprocess:
    """A persistent oracle process reading hex lines, writing JSON."""
    name: str
    process: subprocess.Popen
    stdin: IO[str]
    stdout: IO[str]

    def verify(self, wire: bytes) -> dict[str, Any]:
        self.stdin.write(wire.hex() + "\n")
        self.stdin.flush()
        line = self.stdout.readline()
        if not line:
            raise RuntimeError(
                f"oracle {self.name!r}: EOF while reading report"
            )
        return json.loads(line)

    def close(self) -> None:
        try:
            self.stdin.close()
        except BrokenPipeError:
            pass
        try:
            self.process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.process.kill()


# ---------------------------------------------------------------------------
# Subprocess factories
# ---------------------------------------------------------------------------


def _spawn_cpp(binary: Path) -> OracleSubprocess:
    if not binary.is_file():
        raise FileNotFoundError(
            f"C++ oracle CLI not found at {binary}. Build it with "
            "`cmake --build core-cpp/build --target oigtl_oracle_cli`."
        )
    proc = subprocess.Popen(
        [str(binary)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=sys.stderr,
        text=True,
        bufsize=1,
        # errors='replace' — fuzzer inputs can drive device_name /
        # type_id past ASCII, which some CLIs may emit as raw
        # non-UTF-8 bytes (though the current C++ CLI escapes them).
        # Defence-in-depth: don't crash the runner on a single bad
        # byte.
        encoding="utf-8",
        errors="replace",
    )
    assert proc.stdin is not None and proc.stdout is not None
    return OracleSubprocess(name="cpp", process=proc,
                            stdin=proc.stdin, stdout=proc.stdout)


def _spawn_ts(script: Path) -> OracleSubprocess:
    if not script.is_file():
        raise FileNotFoundError(
            f"TS oracle CLI not found at {script}. Build it with "
            "`(cd core-ts && npx tsc -p tsconfig.json --outDir build-tests)`."
        )
    proc = subprocess.Popen(
        ["node", str(script)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=sys.stderr,
        text=True,
        bufsize=1,
        encoding="utf-8",
        errors="replace",
    )
    assert proc.stdin is not None and proc.stdout is not None
    return OracleSubprocess(name="ts", process=proc,
                            stdin=proc.stdin, stdout=proc.stdout)


def _spawn_py(core_py_dir: Path, no_numpy: bool) -> OracleSubprocess:
    """Spawn the typed Python oracle subprocess.

    Runs ``python -m oigtl.oracle_cli`` from the core-py venv so
    the typed classes and numpy/array.array runtime are exercised.
    The ``OIGTL_NO_NUMPY`` env var toggles the stdlib fallback.
    """
    import os
    venv_python = core_py_dir / ".venv" / "bin" / "python"
    if not venv_python.is_file():
        raise FileNotFoundError(
            f"typed Python oracle needs a core-py venv at "
            f"{venv_python}. Run `(cd core-py && uv sync --all-extras)`."
        )
    env = os.environ.copy()
    if no_numpy:
        env["OIGTL_NO_NUMPY"] = "1"
    else:
        env.pop("OIGTL_NO_NUMPY", None)
    proc = subprocess.Popen(
        [str(venv_python), "-m", "oigtl.oracle_cli"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=sys.stderr,
        text=True,
        bufsize=1,
        encoding="utf-8",
        errors="replace",
        env=env,
    )
    assert proc.stdin is not None and proc.stdout is not None
    name = "py-noarray" if no_numpy else "py"
    return OracleSubprocess(name=name, process=proc,
                            stdin=proc.stdin, stdout=proc.stdout)


# ---------------------------------------------------------------------------
# In-process Python oracle — wraps the reference codec.
# ---------------------------------------------------------------------------


class _PyRefOracle:
    """Same interface as OracleSubprocess, but in-process and fast."""
    name = "py-ref"

    def verify(self, wire: bytes) -> dict[str, Any]:
        result = py_verify(wire, check_crc=True, check_round_trip=True)
        return _result_to_dict(result)

    def close(self) -> None:
        return None


# ---------------------------------------------------------------------------
# Comparison
# ---------------------------------------------------------------------------


def _compare(reports: dict[str, dict[str, Any]]) -> list[str]:
    """Return the list of semantic fields on which oracles disagree.

    Returns an empty list when all oracles agree. If all oracles
    rejected (all ``ok == False``), only the ``ok`` flag is compared
    — error strings are expected to differ.
    """
    names = sorted(reports.keys())
    oks = {name: reports[name]["ok"] for name in names}
    if len(set(oks.values())) > 1:
        return ["ok"]
    if all(v is False for v in oks.values()):
        # All rejected — don't compare semantic fields (they may be
        # partially filled up to the point of failure; divergence
        # there is an artifact, not a bug).
        return []
    # All accepted — every semantic field must match.
    disagreements: list[str] = []
    for field in _SEMANTIC_FIELDS:
        values = {name: reports[name].get(field) for name in names}
        if len(set(json.dumps(v, sort_keys=True) for v in values.values())) > 1:
            disagreements.append(field)
    return disagreements


# ---------------------------------------------------------------------------
# Runner entry point
# ---------------------------------------------------------------------------


@dataclasses.dataclass
class RunnerResult:
    iterations: int
    disagreements: int
    elapsed_sec: float
    per_oracle_rejects: dict[str, int]
    per_generator_iters: dict[str, int]


def run(
    *,
    iterations: int,
    seed: int,
    generators: list[str],
    oracles: list[str],
    cpp_binary: Path | None,
    ts_script: Path | None,
    core_py_dir: Path | None = None,
    disagreements_log: Path | None = None,
    progress_every: int = 1000,
) -> RunnerResult:
    """Run a differential fuzz campaign.

    :param iterations: total number of candidate inputs to generate.
    :param seed: PRNG seed (reproducible runs).
    :param generators: subset of {"random", "mutate", "structured"}.
    :param oracles: subset of {"py-ref", "cpp", "ts"}.
    :param cpp_binary: path to ``oigtl_oracle_cli``; required if
        "cpp" is in *oracles*.
    :param ts_script: path to the compiled ``oracle_cli.js``;
        required if "ts" is in *oracles*.
    :param disagreements_log: path to write disagreement JSON lines.
    :param progress_every: emit a progress line every N iterations.
    """
    for name in generators:
        if name not in GENERATORS:
            raise ValueError(f"unknown generator {name!r}")

    rng = random.Random(seed)

    # Spin up oracles.
    handles: dict[str, Any] = {}
    if "py-ref" in oracles:
        handles["py-ref"] = _PyRefOracle()
    if "py" in oracles:
        if core_py_dir is None:
            raise ValueError("py oracle selected but core_py_dir not provided")
        handles["py"] = _spawn_py(core_py_dir, no_numpy=False)
    if "py-noarray" in oracles:
        if core_py_dir is None:
            raise ValueError(
                "py-noarray oracle selected but core_py_dir not provided"
            )
        handles["py-noarray"] = _spawn_py(core_py_dir, no_numpy=True)
    if "cpp" in oracles:
        if cpp_binary is None:
            raise ValueError("cpp oracle selected but cpp_binary not provided")
        handles["cpp"] = _spawn_cpp(cpp_binary)
    if "ts" in oracles:
        if ts_script is None:
            raise ValueError("ts oracle selected but ts_script not provided")
        handles["ts"] = _spawn_ts(ts_script)

    log_fp: IO[str] | None = None
    if disagreements_log is not None:
        disagreements_log.parent.mkdir(parents=True, exist_ok=True)
        log_fp = disagreements_log.open("w")

    rejects: dict[str, int] = {name: 0 for name in handles}
    per_gen: dict[str, int] = {name: 0 for name in generators}
    disagreements = 0
    t0 = time.perf_counter()

    try:
        for i, (gen_name, wire) in enumerate(
            iter_candidates(rng, generators, iterations), start=1
        ):
            per_gen[gen_name] += 1
            reports: dict[str, dict[str, Any]] = {}
            for name, handle in handles.items():
                report = handle.verify(wire)
                reports[name] = report
                if not report["ok"]:
                    rejects[name] += 1

            diff = _compare(reports)
            if diff:
                disagreements += 1
                if log_fp is not None:
                    log_fp.write(json.dumps({
                        "iteration": i,
                        "generator": gen_name,
                        "wire_hex": wire.hex(),
                        "disagreements": diff,
                        "reports": reports,
                    }) + "\n")
                    log_fp.flush()

            if progress_every > 0 and i % progress_every == 0:
                elapsed = time.perf_counter() - t0
                rate = i / elapsed if elapsed > 0 else 0.0
                print(
                    f"[{i}/{iterations}] {rate:.0f} it/s  "
                    f"disagreements={disagreements}",
                    file=sys.stderr,
                )
    finally:
        for handle in handles.values():
            handle.close()
        if log_fp is not None:
            log_fp.close()

    elapsed = time.perf_counter() - t0
    return RunnerResult(
        iterations=iterations,
        disagreements=disagreements,
        elapsed_sec=elapsed,
        per_oracle_rejects=rejects,
        per_generator_iters=per_gen,
    )
