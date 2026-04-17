"""Jinja2 renderer for the typed TypeScript message codec.

Mirrors :mod:`.python_emit` for the TypeScript target. Produces one
``.ts`` per wire type_id plus an ``index.ts`` that re-exports them,
seeds a ``REGISTRY`` map, and calls ``registerMessage`` for each
class on module load — this populates ``oigtl.runtime.dispatch``
so ``parseMessage`` / ``verifyWireBytes`` work.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any

from jinja2 import Environment, FileSystemLoader, StrictUndefined

from oigtl_corpus_tools.codegen.ts_types import (
    MessageTsPlan,
    _ts_class_name,
    _ts_module_name,
    plan_message,
)


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
class RenderedTsMessage:
    type_id: str
    module_name: str
    text: str


def render_message(schema: dict[str, Any]) -> RenderedTsMessage:
    plan: MessageTsPlan = plan_message(schema)
    env = _make_environment()
    text = env.get_template("ts_message.ts.jinja").render(plan=plan)
    return RenderedTsMessage(
        type_id=plan.type_id,
        module_name=plan.module_name,
        text=text,
    )


@dataclass
class TsRegistryEntry:
    type_id: str
    class_name: str
    module_name: str


@dataclass
class RenderedTsIndex:
    text: str


def render_index(type_ids: list[str]) -> RenderedTsIndex:
    entries = [
        TsRegistryEntry(
            type_id=tid,
            class_name=_ts_class_name(tid),
            module_name=_ts_module_name(tid),
        )
        for tid in sorted(type_ids)
    ]
    env = _make_environment()
    text = env.get_template("ts_index.ts.jinja").render(entries=entries)
    return RenderedTsIndex(text=text)
