"""Jinja2-based renderer for the C-target codec.

Mirrors :mod:`cpp_emit` — thin wrappers that render the planned
message into ``.h`` / ``.c`` text.

The emitter intentionally produces ONLY per-message files. Shared
runtime (crc64, byte_order, header) is hand-written in
``core-c/src/`` and ``core-c/include/oigtl_c/`` and never touched
by codegen. This keeps the hand-written layer inspectable and
gives reviewers a small, auditable surface that never drifts.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any

from jinja2 import Environment, FileSystemLoader, StrictUndefined

from oigtl_corpus_tools.codegen.c_types import MessagePlan, plan_message


_TEMPLATE_DIR = Path(__file__).parent / "templates"


def _make_environment() -> Environment:
    return Environment(
        loader=FileSystemLoader(str(_TEMPLATE_DIR)),
        undefined=StrictUndefined,
        keep_trailing_newline=True,
        trim_blocks=True,
        lstrip_blocks=True,
    )


@dataclass
class RenderedMessage:
    type_id: str
    basename: str
    h_text: str
    c_text: str


def render_message(schema: dict[str, Any]) -> RenderedMessage:
    plan: MessagePlan = plan_message(schema)
    env = _make_environment()
    h = env.get_template("c_message.h.jinja").render(plan=plan)
    c = env.get_template("c_message.c.jinja").render(plan=plan)
    return RenderedMessage(
        type_id=plan.type_id,
        basename=plan.basename,
        h_text=h,
        c_text=c,
    )
