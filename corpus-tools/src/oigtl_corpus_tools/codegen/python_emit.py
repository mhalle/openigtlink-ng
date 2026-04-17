"""Jinja2 renderer for the typed Python message codec.

Mirrors :mod:`.cpp_emit` for the Python target. The Python output
is one ``.py`` per wire type_id plus an ``__init__.py`` that
re-exports them and provides a ``REGISTRY`` dict for type_id → class
lookup. Symmetric to the C++ ``register_all`` story.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any

from jinja2 import Environment, FileSystemLoader, StrictUndefined

from oigtl_corpus_tools.codegen.python_types import (
    MessagePyPlan,
    plan_message,
    py_class_name,
    py_module_name,
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
class RenderedPyMessage:
    type_id: str
    module_name: str
    text: str


def render_message(schema: dict[str, Any]) -> RenderedPyMessage:
    """Render one typed message module from a loaded schema dict."""
    plan: MessagePyPlan = plan_message(schema)
    env = _make_environment()
    text = env.get_template("python_message.py.jinja").render(plan=plan)
    return RenderedPyMessage(
        type_id=plan.type_id,
        module_name=plan.module_name,
        text=text,
    )


@dataclass
class RegistryEntry:
    type_id: str
    class_name: str
    module_name: str


@dataclass
class RenderedPyInit:
    text: str


def render_init(type_ids: list[str]) -> RenderedPyInit:
    """Render the package ``__init__.py`` re-exporting all messages."""
    entries = [
        RegistryEntry(
            type_id=tid,
            class_name=py_class_name(tid),
            module_name=py_module_name(tid),
        )
        for tid in sorted(type_ids)
    ]
    env = _make_environment()
    text = env.get_template("python_init.py.jinja").render(entries=entries)
    return RenderedPyInit(text=text)
