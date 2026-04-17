"""Schema-driven validation policies shared across codec entry points.

These helpers capture spec-level constraints that are not expressible
through the field layout alone, so every code path that takes bytes off
the wire can enforce them uniformly. Keeping the checks here (rather
than inlined at each call site) prevents drift between ``unpack_message``
and the conformance oracle, and gives future code paths a single place
to pick them up.
"""

from __future__ import annotations

from typing import Any


def check_body_size_set(schema: dict[str, Any], body_bytes_len: int) -> None:
    """Raise ValueError if schema declares body_size_set and body_bytes_len is not in it.

    No-op when the schema has no ``body_size_set`` field — the vast
    majority of message types accept any size compatible with their
    field layout. This is a focused guard for the handful of v1 legacy
    messages (POSITION, possibly others in future) whose spec explicitly
    whitelists sizes.

    The check runs before any field-level parsing so out-of-set bodies
    surface as a clean MALFORMED rejection rather than leaking through
    as a truncation of some interior field.
    """
    allowed = schema.get("body_size_set")
    if allowed is None:
        return
    if body_bytes_len not in allowed:
        type_id = schema.get("type_id", "<unknown>")
        raise ValueError(
            f"{type_id} body_size={body_bytes_len} is not in the "
            f"allowed set {sorted(allowed)}"
        )
