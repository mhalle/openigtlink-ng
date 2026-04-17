"""Negative corpus — every entry MUST be rejected by the reference codec.

Entries flagged with ``currently_accepted_by`` containing ``"py-ref"``
are known codec gaps (tracked as followups in
``security/PLAN.md``). They're kept in the corpus as *documentation*
of the correct behaviour; once the gap is fixed the annotation is
removed and the test flips from xfail to pass.
"""

from __future__ import annotations

import json

import pytest

from oigtl_corpus_tools.codec.oracle import verify_wire_bytes
from oigtl_corpus_tools.paths import find_repo_root


# ---------------------------------------------------------------------------
# Load the corpus index
# ---------------------------------------------------------------------------

_REPO_ROOT = find_repo_root()
_NEG_DIR = _REPO_ROOT / "spec" / "corpus" / "negative"
_INDEX = json.loads((_NEG_DIR / "index.json").read_text())
_ENTRIES = _INDEX["entries"]


def _load_entry(name: str) -> bytes:
    return (_NEG_DIR / _ENTRIES[name]["path"]).read_bytes()


# ---------------------------------------------------------------------------
# Per-entry parametrized rejection test
# ---------------------------------------------------------------------------


@pytest.mark.parametrize("name", sorted(_ENTRIES.keys()))
def test_reference_oracle_rejects(name: str) -> None:
    entry = _ENTRIES[name]
    if "py-ref" in entry.get("currently_accepted_by", []):
        pytest.xfail(
            f"known codec gap: {entry.get('known_issue', 'no details')}"
        )
    data = _load_entry(name)
    result = verify_wire_bytes(data)
    assert not result.ok, (
        f"{name}: expected rejection with error_class={entry['error_class']!r}, "
        f"got ok=True (decoded as {result.type_id!r})"
    )


# ---------------------------------------------------------------------------
# Coverage summary — quick sanity check that the suite is wide enough.
# ---------------------------------------------------------------------------


def test_corpus_has_minimum_coverage() -> None:
    """Corpus must cover every enum value in `error_class`."""
    expected = {"SHORT_BUFFER", "CRC_MISMATCH", "MALFORMED"}
    seen = {e["error_class"] for e in _ENTRIES.values()}
    missing = expected - seen
    assert not missing, f"no entries for error_class(es): {missing}"


def test_corpus_index_is_drift_free() -> None:
    """Running the generator and comparing to the on-disk file must not drift."""
    from oigtl_corpus_tools.negative_corpus import build_corpus, index_payload

    expected = json.dumps(index_payload(build_corpus()), indent=2) + "\n"
    actual = (_NEG_DIR / "index.json").read_text()
    assert expected == actual, (
        "spec/corpus/negative/index.json is out of date. Run "
        "`oigtl-corpus corpus generate-negative`."
    )
