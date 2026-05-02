"""``messages-doc`` command — render ``spec/MESSAGES.md`` from schemas.

The 84 message-schema files under ``spec/schemas/`` carry rich
metadata fields (``description``, ``rationale``, ``spec_reference``,
``introduced_in``, ``legacy_notes``, ``see_also``, etc.) — enough
to produce a comprehensive human-readable reference deterministically.
This command does that.

Output: ``spec/MESSAGES.md``, a single Markdown file with one
section per message type, an index, and cross-links via
``see_also``.

With ``--check``, compare the generated content against the
committed file and exit non-zero if they differ. Drift detection
mirrors the pattern used by ``schema emit-meta`` and the codegen
``--check`` modes.

Categorization:

- ``framing_*`` (header / extended-header / metadata / unit) →
  framing structures.
- ``GET_*`` / ``STT_*`` / ``STP_*`` / ``RTS_*`` → query and
  control messages, grouped by their kind.
- Everything else → data messages.

Output is deliberately wordy where the schemas are wordy: a
reference doc is meant to absorb prose, not summarise it.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections.abc import Iterable
from dataclasses import dataclass
from pathlib import Path

from oigtl_corpus_tools.paths import RepoRootNotFound, find_repo_root
from oigtl_corpus_tools.schema.field import FieldSchema
from oigtl_corpus_tools.schema.message import MessageSchema


# ---------------------------------------------------------------------------
# CLI registration
# ---------------------------------------------------------------------------


def register(parser: argparse.ArgumentParser) -> None:
    parser.set_defaults(handler=_cmd)
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help=(
            "Path to write the generated reference. Defaults to "
            "<repo>/spec/MESSAGES.md."
        ),
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help=(
            "Verify that the committed file matches the generated "
            "content. Does not write. Non-zero exit if out of sync."
        ),
    )


# ---------------------------------------------------------------------------
# Handler
# ---------------------------------------------------------------------------


def _cmd(args: argparse.Namespace) -> int:
    try:
        repo_root = find_repo_root()
    except RepoRootNotFound as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    schema_dir = repo_root / "spec" / "schemas"
    out_path = args.output or (repo_root / "spec" / "MESSAGES.md")

    schemas = _load_schemas(schema_dir)
    rendered = render(schemas)

    if args.check:
        try:
            committed = out_path.read_text()
        except FileNotFoundError:
            print(
                f"error: {out_path} does not exist; run "
                f"`oigtl-corpus messages-doc` to create it.",
                file=sys.stderr,
            )
            return 1
        if committed != rendered:
            print(
                f"error: {out_path} is out of sync with the schemas. "
                f"Run `oigtl-corpus messages-doc` and commit the "
                f"result.",
                file=sys.stderr,
            )
            return 1
        print(f"ok: {out_path} is in sync with {schema_dir}/.")
        return 0

    out_path.write_text(rendered)
    print(f"wrote: {out_path}  ({len(schemas)} message types)")
    return 0


# ---------------------------------------------------------------------------
# Loading + categorisation
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class LoadedSchema:
    """A schema together with its source filename (for stable ordering)."""

    filename: str
    schema: MessageSchema


def _load_schemas(schema_dir: Path) -> list[LoadedSchema]:
    files = sorted(schema_dir.glob("*.json"))
    if not files:
        raise FileNotFoundError(
            f"no *.json files under {schema_dir}; wrong directory?"
        )
    out: list[LoadedSchema] = []
    for f in files:
        try:
            data = json.loads(f.read_text())
        except json.JSONDecodeError as exc:
            raise ValueError(f"{f}: invalid JSON: {exc}") from exc
        # Strip the JSON Schema $schema pointer; it's a hint for
        # non-Python tooling, not a model field. Same treatment as
        # the `schema validate` command.
        data = {k: v for k, v in data.items() if k != "$schema"}
        try:
            schema = MessageSchema.model_validate(data)
        except Exception as exc:
            raise ValueError(f"{f}: schema validation failed: {exc}") from exc
        out.append(LoadedSchema(filename=f.name, schema=schema))
    return out


# Categorisation: file-prefix → bucket. The order here is the order
# sections appear in the rendered output.
_BUCKET_ORDER = (
    "data",
    "get",
    "stt",
    "stp",
    "rts",
    "framing",
)
_BUCKET_TITLES = {
    "data": "Data messages",
    "get": "Query messages — `GET_*`",
    "stt": "Stream-start messages — `STT_*`",
    "stp": "Stream-stop messages — `STP_*`",
    "rts": "Response messages — `RTS_*`",
    "framing": "Framing structures",
}
_BUCKET_BLURBS = {
    "data": (
        "Carry payloads of typed data on the wire. The bulk of "
        "OpenIGTLink traffic — pose, image, sensor, status, command."
    ),
    "get": (
        "Request a single snapshot of a data message type. The peer "
        "responds with the matching data message."
    ),
    "stt": (
        "Open a streaming subscription to a data message type. The "
        "peer publishes data messages until told to stop."
    ),
    "stp": (
        "Close a streaming subscription opened by a `STT_*` message."
    ),
    "rts": (
        "Status replies to streaming-control commands. Carry an "
        "outcome code that the requester acts on."
    ),
    "framing": (
        "Wire-level structural records — not user-facing message "
        "types. Documented here for reference because their schemas "
        "are part of the spec."
    ),
}


def _bucket_for(loaded: LoadedSchema) -> str:
    name = loaded.filename.removesuffix(".json")
    for prefix in ("framing", "get", "stt", "stp", "rts"):
        if name == prefix or name.startswith(prefix + "_"):
            return prefix
    return "data"


# ---------------------------------------------------------------------------
# Rendering
# ---------------------------------------------------------------------------


_HEADER = """\
# OpenIGTLink Message Reference

> **This file is generated** from `spec/schemas/*.json` by
> `oigtl-corpus messages-doc`. Do not edit by hand — re-run the
> generator and commit the output. CI verifies it stays in sync
> on every PR.

This is the reference for every message type and framing structure
defined by the OpenIGTLink protocol. For the protocol itself (wire
format, framing, header layout, CRC, transport), see
[`protocol/v3.md`](protocol/v3.md). For the schemas this file is
generated from, see [`schemas/`](schemas/).
"""


def render(schemas: Iterable[LoadedSchema]) -> str:
    schemas = list(schemas)

    # Bucket each schema, keeping per-bucket lists in filename order
    # so the output is deterministic.
    buckets: dict[str, list[LoadedSchema]] = {b: [] for b in _BUCKET_ORDER}
    for s in schemas:
        buckets[_bucket_for(s)].append(s)

    parts: list[str] = [_HEADER, ""]

    parts.append("## Index\n")
    for bucket in _BUCKET_ORDER:
        items = buckets[bucket]
        if not items:
            continue
        parts.append(f"### {_BUCKET_TITLES[bucket]}\n")
        parts.append(_BUCKET_BLURBS[bucket] + "\n")
        for item in items:
            type_id = item.schema.type_id
            anchor = _anchor_for(type_id)
            parts.append(f"- [`{type_id}`](#{anchor}) — {item.schema.description.split('. ')[0].rstrip('.')}.")
        parts.append("")

    parts.append("---\n")

    for bucket in _BUCKET_ORDER:
        items = buckets[bucket]
        if not items:
            continue
        parts.append(f"## {_BUCKET_TITLES[bucket]}\n")
        for item in items:
            parts.append(_render_message(item.schema))

    return "\n".join(parts).rstrip() + "\n"


def _anchor_for(type_id: str) -> str:
    """GitHub-style heading anchor for a type_id."""
    # GitHub lower-cases and replaces underscores with hyphens; for
    # OpenIGTLink type_ids, this is sufficient.
    return type_id.lower().replace("_", "-")


def _render_message(m: MessageSchema) -> str:
    parts: list[str] = []

    # Heading + summary box
    parts.append(f"### `{m.type_id}`\n")
    summary_rows = [
        f"**Type ID:** `{m.type_id}`",
        f"**Introduced in:** {_enum_value(m.introduced_in)}",
    ]
    if m.deprecated_in:
        summary_rows.append(
            f"**Deprecated in:** {_enum_value(m.deprecated_in)}",
        )
    summary_rows.append(f"**Body size:** {_format_body_size(m)}")
    if m.metadata_allowed is not False:
        summary_rows.append("**Metadata allowed:** yes (v2 / v3)")
    else:
        summary_rows.append("**Metadata allowed:** no")
    parts.append(" &nbsp;·&nbsp; ".join(summary_rows) + "\n")

    # Description
    parts.append(m.description + "\n")

    # Rationale
    if m.rationale:
        parts.append("**Rationale.** " + m.rationale + "\n")

    # Fields
    if m.fields:
        parts.append("**Fields.**\n")
        parts.append(_render_fields_table(m.fields))
        parts.append("")
        # Per-field rationales / notes worth surfacing
        for f in m.fields:
            extras = _render_field_extras(f)
            if extras:
                parts.append(extras)

    # Post-unpack invariant
    if m.post_unpack_invariant:
        parts.append(
            f"**Post-unpack invariant.** `{m.post_unpack_invariant}` "
            f"— see `corpus-tools/src/oigtl_corpus_tools/codec/policy.py` "
            f"for the cross-codec invariant definition.\n"
        )

    # Body-size set
    if m.body_size_set:
        legal = ", ".join(str(n) for n in m.body_size_set)
        parts.append(
            f"**Legal body sizes.** {legal} bytes only. Codecs reject "
            f"any other length before field access.\n"
        )

    # Legacy notes
    if m.legacy_notes:
        parts.append("**Legacy notes.**\n")
        for note in m.legacy_notes:
            parts.append(f"- {note}")
        parts.append("")

    # Other notes
    if m.notes:
        parts.append("**Notes.**\n")
        for note in m.notes:
            parts.append(f"- {note}")
        parts.append("")

    # See also
    if m.see_also:
        links = ", ".join(
            f"[`{ref}`](#{_anchor_for(ref)})" for ref in m.see_also
        )
        parts.append(f"**See also.** {links}\n")

    # Spec reference
    if m.spec_reference:
        section = m.spec_reference.section
        document = m.spec_reference.document
        parts.append(
            f"**Spec reference.** [{document} §\"{section}\"]"
            f"({document})\n"
        )

    parts.append("---\n")
    return "\n".join(parts)


def _format_body_size(m: MessageSchema) -> str:
    if m.body_size == "variable":
        return "variable"
    return f"{m.body_size} bytes"


def _render_fields_table(fields: list[FieldSchema]) -> str:
    rows = ["| Name | Type | Size | Description |", "|---|---|---|---|"]
    for f in fields:
        rows.append(
            "| `{name}` | {type_} | {size} | {desc} |".format(
                name=f.name,
                type_=_format_field_type(f),
                size=_format_field_size(f),
                desc=_one_line(f.description),
            )
        )
    return "\n".join(rows)


def _format_field_type(f: FieldSchema) -> str:
    base = _enum_value(f.type)
    if f.element_type:
        elt = _enum_value(f.element_type)
        if f.count is not None:
            return f"{elt} × {f.count}"
        if f.count_from:
            return f"{elt} × ({_enum_value(f.count_from)})"
        return f"{base}<{elt}>"
    return base


def _enum_value(value) -> str:
    """Render Pydantic enum members as their wire value, not Python repr."""
    inner = getattr(value, "value", value)
    return str(inner)


def _format_field_size(f: FieldSchema) -> str:
    if f.size_bytes is not None:
        return f"{f.size_bytes} B" if isinstance(f.size_bytes, int) else str(f.size_bytes)
    return "—"


def _render_field_extras(f: FieldSchema) -> str:
    """Render rationale / introduced_in / legacy_notes / endianness for a field."""
    bits: list[str] = []
    if f.rationale:
        bits.append(f"  - **`{f.name}` rationale.** {f.rationale}")
    if f.introduced_in:
        bits.append(
            f"  - **`{f.name}` introduced in:** "
            f"{_enum_value(f.introduced_in)}."
        )
    if f.endianness:
        bits.append(
            f"  - **`{f.name}` endianness:** {_enum_value(f.endianness)}."
        )
    if getattr(f, "layout", None):
        bits.append(f"  - **`{f.name}` layout:** `{f.layout}`.")
    if getattr(f, "legacy_notes", None):
        for note in f.legacy_notes:
            bits.append(f"  - **`{f.name}` legacy.** {note}")
    if not bits:
        return ""
    # Trailing blank line separates this field's extras from
    # whatever follows (next field's extras or the next H4).
    return "\n".join(bits) + "\n"


def _one_line(s: str) -> str:
    """Collapse whitespace so a description fits in a Markdown table cell."""
    return " ".join(s.split())
