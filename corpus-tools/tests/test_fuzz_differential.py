"""Smoke tests for the differential oracle fuzzer.

Runs a short campaign end-to-end to exercise:

- Generator plumbing (random / mutate / structured).
- Comparison logic (agreement path; disagreement path is exercised
  by regression tests that feed a known-divergent input).
- CLI exit code behaviour.

Heavy fuzzing (millions of iterations) is done outside pytest via
``oigtl-corpus fuzz differential`` — this is a correctness test, not
a coverage driver.
"""

from __future__ import annotations

import random

import pytest

from oigtl_corpus_tools.fuzz import runner
from oigtl_corpus_tools.fuzz.generators import (
    GENERATORS,
    mutate_fixture,
    random_bytes,
    structured_header,
)


class TestGenerators:
    def test_random_respects_max_len(self):
        rng = random.Random(0)
        for _ in range(200):
            out = random_bytes(rng, max_len=128)
            assert 0 <= len(out) <= 128

    def test_mutate_preserves_relationship_to_fixture(self):
        rng = random.Random(1)
        # Should produce non-empty bytes; mutation kinds are independent.
        for _ in range(50):
            out = mutate_fixture(rng)
            assert isinstance(out, bytes)
            assert len(out) > 0

    def test_structured_header_is_at_least_58_bytes(self):
        rng = random.Random(2)
        for _ in range(50):
            out = structured_header(rng)
            assert len(out) >= 58

    def test_generator_registry_matches_cli_choices(self):
        assert set(GENERATORS.keys()) == {"random", "mutate", "structured"}


class TestRunner:
    """End-to-end py-ref-only run. Cross-language run lives outside
    pytest because it requires the C++ binary + TS JS to be built."""

    def test_py_ref_only_agrees_with_itself(self, tmp_path):
        log = tmp_path / "disagreements.jsonl"
        result = runner.run(
            iterations=200,
            seed=42,
            generators=["random", "mutate", "structured"],
            oracles=["py-ref"],
            cpp_binary=None,
            ts_script=None,
            disagreements_log=log,
            progress_every=0,
        )
        assert result.iterations == 200
        # Single oracle can't disagree with itself.
        assert result.disagreements == 0
        # Each generator got a ~equal share.
        for name, count in result.per_generator_iters.items():
            assert 60 <= count <= 80, name

    def test_runner_rejects_unknown_generator(self, tmp_path):
        with pytest.raises(ValueError, match="unknown generator"):
            runner.run(
                iterations=10,
                seed=0,
                generators=["bogus"],
                oracles=["py-ref"],
                cpp_binary=None,
                ts_script=None,
                disagreements_log=tmp_path / "log.jsonl",
                progress_every=0,
            )

    def test_runner_requires_cpp_binary_when_cpp_oracle_selected(self, tmp_path):
        with pytest.raises(ValueError, match="cpp_binary"):
            runner.run(
                iterations=10,
                seed=0,
                generators=["random"],
                oracles=["py-ref", "cpp"],
                cpp_binary=None,
                ts_script=None,
                disagreements_log=tmp_path / "log.jsonl",
                progress_every=0,
            )

    def test_runner_requires_core_py_dir_when_py_oracle_selected(self, tmp_path):
        with pytest.raises(ValueError, match="core_py_dir"):
            runner.run(
                iterations=10,
                seed=0,
                generators=["random"],
                oracles=["py-ref", "py"],
                cpp_binary=None,
                ts_script=None,
                core_py_dir=None,
                disagreements_log=tmp_path / "log.jsonl",
                progress_every=0,
            )
