"""Smoke tests for the C++ codegen.

These tests pin two properties:

1. The codegen runs end-to-end on the TRANSFORM schema and produces
   non-empty .hpp / .cpp text containing the expected class.
2. The on-disk generated files in core-cpp/ are in sync with what
   the codegen would emit — i.e. nobody hand-edited a generated file
   and forgot to update the schema or template.

The actual *correctness* of the generated codec is validated by the
C++ test binary (core-cpp/tests/upstream_fixtures_test.cpp) running
under CTest, which round-trips the upstream fixture byte-for-byte.
"""

from __future__ import annotations

import json

import pytest

from oigtl_corpus_tools.codegen.cpp_emit import render_message
from oigtl_corpus_tools.codegen.cpp_types import (
    cxx_class_name,
    plan_message,
)
from oigtl_corpus_tools.paths import find_repo_root, schemas_dir


def _load(type_id: str) -> dict:
    sdir = schemas_dir(find_repo_root())
    for path in sdir.glob("*.json"):
        with open(path) as f:
            schema = json.load(f)
        if schema.get("type_id") == type_id:
            return schema
    raise FileNotFoundError(f"no schema with type_id={type_id!r}")


def test_class_name_mapping():
    assert cxx_class_name("TRANSFORM") == "Transform"
    assert cxx_class_name("STT_TRANSFORM") == "SttTransform"
    assert cxx_class_name("VIDEOMETA") == "Videometa"


def test_plan_transform_body_size():
    plan = plan_message(_load("TRANSFORM"))
    assert plan.body_size == 48
    assert len(plan.fields) == 1
    assert plan.fields[0].cxx_type == "std::array<float, 12>"


def test_render_transform_contains_expected_symbols():
    rendered = render_message(_load("TRANSFORM"))
    assert "struct Transform" in rendered.hpp_text
    assert 'kTypeId = "TRANSFORM"' in rendered.hpp_text
    assert "kBodySize = 48" in rendered.hpp_text
    assert "Transform::pack" in rendered.cpp_text
    assert "Transform::unpack" in rendered.cpp_text
    assert "GENERATED" in rendered.hpp_text
    assert "GENERATED" in rendered.cpp_text


def test_generated_files_in_sync_with_schema():
    """The committed core-cpp/ generated files match what we'd emit now."""
    repo_root = find_repo_root()
    rendered = render_message(_load("TRANSFORM"))

    hpp = repo_root / "core-cpp/include/oigtl/messages/transform.hpp"
    cpp = repo_root / "core-cpp/src/messages/transform.cpp"
    if not hpp.is_file() or not cpp.is_file():
        pytest.skip("generated files not yet emitted (Phase 2 scaffold)")

    assert hpp.read_text() == rendered.hpp_text, (
        "transform.hpp drifted from codegen output — "
        "run 'uv run oigtl-corpus codegen cpp --type-id TRANSFORM'"
    )
    assert cpp.read_text() == rendered.cpp_text, (
        "transform.cpp drifted from codegen output — "
        "run 'uv run oigtl-corpus codegen cpp --type-id TRANSFORM'"
    )
