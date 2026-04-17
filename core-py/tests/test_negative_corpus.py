"""Negative corpus — the typed library must reject every must-reject input.

Mirrors ``corpus-tools/tests/test_negative_corpus.py`` against the
typed ``oigtl.runtime.oracle`` wrapper. Both codecs should agree on
which entries are rejected; disagreements are first-class bugs.

Entries flagged with ``currently_accepted_by`` containing ``"py"``
are known codec gaps tracked as followups in
``security/PLAN.md``.
"""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from oigtl.runtime.oracle import verify_wire_bytes


def _find_repo_root() -> Path:
    """Walk upward until we find a directory containing `spec/`."""
    here = Path(__file__).resolve()
    for parent in (here, *here.parents):
        if (parent / "spec" / "corpus" / "negative" / "index.json").is_file():
            return parent
    raise RuntimeError(f"repo root not found from {here}")


_REPO_ROOT = _find_repo_root()
_NEG_DIR = _REPO_ROOT / "spec" / "corpus" / "negative"
_INDEX = json.loads((_NEG_DIR / "index.json").read_text())
_ENTRIES = _INDEX["entries"]


@pytest.mark.parametrize("name", sorted(_ENTRIES.keys()))
def test_typed_oracle_rejects(name: str) -> None:
    entry = _ENTRIES[name]
    if "py" in entry.get("currently_accepted_by", []):
        pytest.xfail(
            f"known codec gap: {entry.get('known_issue', 'no details')}"
        )
    data = (_NEG_DIR / entry["path"]).read_bytes()
    result = verify_wire_bytes(data)
    assert not result.ok, (
        f"{name}: expected rejection with error_class={entry['error_class']!r}, "
        f"got ok=True"
    )
