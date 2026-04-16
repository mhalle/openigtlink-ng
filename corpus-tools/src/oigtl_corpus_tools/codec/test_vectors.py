"""Extract upstream test vectors from C header files.

Each ``igtl_test_data_*.h`` file in the upstream ``Testing/igtlutil/``
directory contains one or more ``unsigned char test_*_message[] = { ... };``
arrays. This module parses them into Python bytes objects at import time,
making them available to the test suite and oracle.
"""

from __future__ import annotations

import re
from pathlib import Path
from typing import Iterator

from oigtl_corpus_tools.paths import find_repo_root


_C_COMMENT_RE = re.compile(r"/\*.*?\*/|//[^\n]*", re.DOTALL)


def _extract_hex_arrays(path: Path) -> Iterator[tuple[str, bytes]]:
    """Yield ``(array_name, raw_bytes)`` for each C array in *path*.

    Strips C comments first so embedded braces (e.g. ``/* size = {5,4,3} */``)
    don't break array boundary detection. Matches both ``unsigned char``
    and ``unsigned const char``.
    """
    text = _C_COMMENT_RE.sub("", path.read_text(errors="replace"))
    pattern = re.compile(
        r"unsigned\s+(?:const\s+)?char\s+(\w+)\s*\[\s*\]\s*=\s*\{([^}]+)\};",
        re.DOTALL,
    )
    for match in pattern.finditer(text):
        name = match.group(1)
        body = match.group(2)
        hex_values = re.findall(r"0x([0-9A-Fa-f]{2})", body)
        raw = bytes(int(h, 16) for h in hex_values)
        yield name, raw


def _looks_like_wire_message(raw: bytes) -> bool:
    """Heuristic: does *raw* look like a complete OpenIGTLink wire message?

    Checks:
    - at least 58 bytes (the fixed header)
    - version field is 1, 2, or 3
    - body_size matches len(raw) - 58

    Used as a fallback when the array name doesn't match a canonical
    pattern — e.g. the Format2 fixture arrays which have ad-hoc names
    like ``test_transform_message_Format2``.
    """
    if len(raw) < 58:
        return False
    version = int.from_bytes(raw[0:2], "big")
    if version not in (1, 2, 3):
        return False
    body_size = int.from_bytes(raw[42:50], "big")
    return len(raw) == 58 + body_size


def _concat_split_vector(arrays: dict[str, bytes], stem: str) -> bytes | None:
    """Try to reassemble a full wire message from split _header/_body arrays.

    Upstream splits multi-section messages (NDARRAY, POLYDATA, BIND,
    etc.) into separate arrays: ``test_<stem>_message_header``,
    ``test_<stem>_message_body``, and for BIND also
    ``test_<stem>_message_bind_header`` and
    ``test_<stem>_message_bind_body``.

    Returns the concatenated bytes in the canonical order, or ``None``
    if the expected parts aren't present.
    """
    # BIND has three parts
    header = arrays.get(f"test_{stem}_message_header")
    bind_header = arrays.get(f"test_{stem}_message_bind_header")
    bind_body = arrays.get(f"test_{stem}_message_bind_body")
    if header and bind_header and bind_body:
        return header + bind_header + bind_body

    # Two-part (NDARRAY, POLYDATA, etc.)
    body = arrays.get(f"test_{stem}_message_body")
    if header and body:
        return header + body

    return None


def _discover_test_vectors() -> dict[str, bytes]:
    """Scan upstream test data and return a name → bytes mapping.

    For single-array files (``test_<stem>_message[]``), returns
    ``{stem: bytes}``. For split-array files (``test_<stem>_message_header[]``
    + ``test_<stem>_message_body[]``), reassembles into a single
    full-wire-message entry under ``stem``.

    Files that contain data but no complete message (e.g.
    ``igtl_test_data_image.h`` holds only raw pixels for constructing
    an IMAGE at runtime) produce no entries.
    """
    try:
        repo = find_repo_root()
    except Exception:
        return {}

    test_dir = (
        repo
        / "corpus-tools"
        / "reference-libs"
        / "openigtlink-upstream"
        / "Testing"
        / "igtlutil"
    )
    if not test_dir.is_dir():
        return {}

    vectors: dict[str, bytes] = {}
    for path in sorted(test_dir.glob("igtl_test_data_*.h")):
        stem = path.stem.removeprefix("igtl_test_data_")
        # Collect all arrays in this file
        arrays: dict[str, bytes] = dict(_extract_hex_arrays(path))
        if not arrays:
            continue

        # Preferred: single canonical array test_<stem>_message
        single = arrays.get(f"test_{stem}_message")
        if single:
            vectors[stem] = single
            continue

        # Split header/body pattern
        joined = _concat_split_vector(arrays, stem)
        if joined:
            vectors[stem] = joined
            continue

        # Fallback: any array in this file that looks like a complete
        # wire message. Handles ad-hoc names like
        # ``test_transform_message_Format2`` that don't match the
        # canonical ``test_<stem>_message`` pattern.
        candidates = [raw for raw in arrays.values() if _looks_like_wire_message(raw)]
        if len(candidates) == 1:
            vectors[stem] = candidates[0]
            continue

        # Otherwise, skip — these files contain partial payload data
        # (e.g. raw pixel arrays for IMAGE construction) rather than
        # complete wire messages.

    return vectors


# Eagerly discover at import time so pytest collection sees them.
UPSTREAM_VECTORS: dict[str, bytes] = _discover_test_vectors()
