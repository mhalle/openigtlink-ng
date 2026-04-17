"""Jinja2-based renderer for C++ wire codec source files.

Inputs are loaded schema dicts (the same JSON the codec uses).
Outputs are :class:`RenderedMessage` instances containing the
``.hpp`` and ``.cpp`` text plus the relative paths they should be
written to.

The split between :mod:`.cpp_types` (planning) and this module
(rendering) is deliberate: planning is testable in pure Python with
no Jinja round-trip, and the templates are kept thin enough that
"render" is mostly text formatting with no embedded logic.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any

from jinja2 import Environment, FileSystemLoader, StrictUndefined

from dataclasses import dataclass

from oigtl_corpus_tools.codegen.cpp_types import (
    MessagePlan,
    cxx_class_name,
    header_basename,
    plan_message,
)


_TEMPLATE_DIR = Path(__file__).parent / "templates"


def _make_environment() -> Environment:
    """Construct the Jinja2 Environment used by all renderers.

    ``StrictUndefined`` makes any typo in a template fail loudly
    rather than silently producing empty strings — important for code
    generation, where a missing variable would otherwise compile to
    bogus C++.
    """
    return Environment(
        loader=FileSystemLoader(str(_TEMPLATE_DIR)),
        undefined=StrictUndefined,
        keep_trailing_newline=True,
        # trim_blocks: drop the newline after a block tag.
        # lstrip_blocks: drop leading whitespace before a block tag.
        # Together, block tags occupy their own line cleanly without
        # adding spurious blank lines or eating sibling indentation.
        trim_blocks=True,
        lstrip_blocks=True,
    )


@dataclass
class RenderedMessage:
    """The two C++ source files for one message type."""

    type_id: str
    header_basename: str    # e.g. "transform"
    hpp_text: str
    cpp_text: str


def render_message(schema: dict[str, Any]) -> RenderedMessage:
    """Render a single message's ``.hpp`` and ``.cpp`` from its schema.

    Raises :class:`NotImplementedError` if the schema uses a field
    shape the current codegen does not yet support (Phase 2 covers
    primitives + fixed-count primitive arrays).
    """
    plan: MessagePlan = plan_message(schema)
    env = _make_environment()
    hpp = env.get_template("message.hpp.jinja").render(plan=plan)
    cpp = env.get_template("message.cpp.jinja").render(plan=plan)
    return RenderedMessage(
        type_id=plan.type_id,
        header_basename=plan.header_basename,
        hpp_text=hpp,
        cpp_text=cpp,
    )


def write_message(
    schema: dict[str, Any],
    *,
    include_dir: Path,
    src_dir: Path,
) -> tuple[Path, Path]:
    """Render *schema* and write both files. Returns ``(hpp_path, cpp_path)``."""
    rendered = render_message(schema)
    include_dir.mkdir(parents=True, exist_ok=True)
    src_dir.mkdir(parents=True, exist_ok=True)
    hpp_path = include_dir / f"{rendered.header_basename}.hpp"
    cpp_path = src_dir / f"{rendered.header_basename}.cpp"
    hpp_path.write_text(rendered.hpp_text)
    cpp_path.write_text(rendered.cpp_text)
    return hpp_path, cpp_path


# ---------------------------------------------------------------------------
# register_all.{hpp,cpp} — the dispatch registry populator
# ---------------------------------------------------------------------------


@dataclass
class RegistryEntry:
    """One row in the generated register_all() table."""
    type_id: str
    class_name: str
    header_basename: str


@dataclass
class RenderedRegistry:
    """The two files for the dispatch registry populator."""
    hpp_text: str
    cpp_text: str


def render_register_all(type_ids: list[str]) -> RenderedRegistry:
    """Render the register_all.{hpp,cpp} pair for *type_ids*.

    *type_ids* is the (already-deduplicated, codegen-supported)
    list of wire type_ids to register. The output sorts them
    alphabetically for stable diffs across regenerations.
    """
    entries = [
        RegistryEntry(
            type_id=tid,
            class_name=cxx_class_name(tid),
            header_basename=header_basename(tid),
        )
        for tid in sorted(type_ids)
    ]
    env = _make_environment()
    hpp = env.get_template("register_all.hpp.jinja").render(entries=entries)
    cpp = env.get_template("register_all.cpp.jinja").render(entries=entries)
    return RenderedRegistry(hpp_text=hpp, cpp_text=cpp)
