"""Schema-type → Python-type mapping for the typed message codegen.

The Python target is significantly simpler than the C++ one:

- No need for per-field pack/unpack code emission. The generated
  classes delegate to ``oigtl_corpus_tools.codec.fields.pack_fields``
  / ``unpack_fields``, which already walks the schema. The codegen's
  job is to (a) produce typed Pydantic field declarations and
  (b) bake the schema fields list into the generated module as a
  Python literal so the runtime library has no filesystem
  dependency on ``spec/schemas/``.

- No nested struct codegen — Pydantic resolves nested
  ``BaseModel`` fields recursively at ``model_validate`` time, so
  we only emit nested classes for human readability + IDE help.

The output for a TRANSFORM-class message is roughly::

    class Transform(BaseModel):
        TYPE_ID: ClassVar[str] = "TRANSFORM"
        BODY_SIZE: ClassVar[int] = 48
        matrix: Annotated[list[float], Field(min_length=12, max_length=12)] = ...

        def pack(self) -> bytes: ...
        @classmethod
        def unpack(cls, data: bytes) -> "Transform": ...

Field shape support mirrors the C++ codegen: primitives,
fixed/length-prefixed/trailing strings, fixed/sibling/remaining
arrays of primitives, fixed-string element arrays, and struct
element arrays.
"""

from __future__ import annotations

import pprint
from dataclasses import dataclass, field
from typing import Any, Optional


# ---------------------------------------------------------------------------
# Primitive type mapping
# ---------------------------------------------------------------------------

# Schema-type name → (Python type expression, default literal).
_PRIMITIVE_PY: dict[str, tuple[str, str]] = {
    "uint8":   ("int",   "0"),
    "uint16":  ("int",   "0"),
    "uint32":  ("int",   "0"),
    "uint64":  ("int",   "0"),
    "int8":    ("int",   "0"),
    "int16":   ("int",   "0"),
    "int32":   ("int",   "0"),
    "int64":   ("int",   "0"),
    "float32": ("float", "0.0"),
    "float64": ("float", "0.0"),
}


def py_primitive_type(name: str) -> str:
    return _PRIMITIVE_PY[name][0]


def py_primitive_default(name: str) -> str:
    return _PRIMITIVE_PY[name][1]


# ---------------------------------------------------------------------------
# Naming
# ---------------------------------------------------------------------------


def py_class_name(type_id: str) -> str:
    """``"TRANSFORM"`` → ``"Transform"``, ``"STT_TRANSFORM"`` → ``"SttTransform"``."""
    return "".join(part.capitalize() for part in type_id.split("_"))


def py_module_name(type_id: str) -> str:
    """Module file stem — lowercase form of type_id."""
    return type_id.lower()


def _singularize(word: str) -> str:
    if word.endswith("ies") and len(word) > 3:
        return word[:-3] + "y"
    if word.endswith("s") and not word.endswith("ss") and len(word) > 1:
        return word[:-1]
    return word


def nested_class_name(field_name: str) -> str:
    parts = field_name.split("_")
    parts[-1] = _singularize(parts[-1])
    return "".join(p.capitalize() for p in parts)


# ---------------------------------------------------------------------------
# Render-ready dataclasses
# ---------------------------------------------------------------------------


@dataclass
class NestedField:
    """One sub-field declaration inside a nested Pydantic model."""
    name: str
    py_type: str           # Pydantic field type expression
    default: str           # Pydantic Field(...) call as a string


@dataclass
class NestedClass:
    """A nested Pydantic class emitted on the parent message."""
    name: str
    fields: list[NestedField]


@dataclass
class TopField:
    """One top-level message field."""
    name: str
    py_type: str
    default: str
    # When set, the field carries a variable-count primitive array
    # (non-uint8). The generated class emits a @field_validator that
    # calls ``coerce_variable_array`` on the incoming value, and its
    # ``pack()`` method pre-converts back to bytes via
    # ``pack_variable_array`` before handing off to the codec.
    variable_primitive_et: Optional[str] = None


@dataclass
class MessagePyPlan:
    """Full plan for one generated module."""
    type_id: str
    class_name: str
    module_name: str
    body_size: Optional[int]
    is_fixed_body: bool
    fields: list[TopField]
    nested_classes: list[NestedClass]
    schema_literal: str         # Python repr of the fields-list,
                                # baked into the generated module.
    needs_arrays_runtime: bool = False  # any variable_primitive_et field
    body_size_set: Optional[list[int]] = None  # spec-whitelist body sizes
                                # (POSITION: {12, 24, 28}); None = any size.


# ---------------------------------------------------------------------------
# Field-shape rendering
# ---------------------------------------------------------------------------


def _render_pydantic_field(
    py_type_inner: str,
    *,
    default_factory: Optional[str] = None,
    default: Optional[str] = None,
    constraints: Optional[list[str]] = None,
) -> tuple[str, str]:
    """Build (py_type_str, default_str) for a Pydantic field.

    *constraints* is a list of ``"min_length=12"``-style fragments
    appended to a ``Field(...)`` call. The annotated form is used
    only when constraints are present, since bare types render
    cleaner without it.
    """
    constraints = constraints or []
    field_args: list[str] = []
    if default_factory is not None:
        field_args.append(f"default_factory={default_factory}")
    elif default is not None:
        field_args.append(f"default={default}")
    field_args.extend(constraints)

    if constraints:
        py_type_str = (
            f"Annotated[{py_type_inner}, "
            f"Field({', '.join(c for c in constraints)})]"
        )
        default_str = (
            f"Field({', '.join(field_args)})" if field_args
            else "Field(...)"
        )
    else:
        py_type_str = py_type_inner
        if default_factory is not None:
            default_str = f"Field(default_factory={default_factory})"
        elif default is not None:
            default_str = default
        else:
            default_str = ""

    return py_type_str, default_str


def _plan_nested_field(sub: dict[str, Any]) -> NestedField:
    """Plan a sub-field inside a struct element."""
    t = sub["type"]
    name = sub["name"]

    if t in _PRIMITIVE_PY:
        py_t, default = _render_pydantic_field(
            py_primitive_type(t), default=py_primitive_default(t),
        )
        return NestedField(name=name, py_type=py_t, default=default)

    if t == "fixed_string":
        py_t, default = _render_pydantic_field("str", default='""')
        return NestedField(name=name, py_type=py_t, default=default)

    if t == "array":
        et = sub.get("element_type")
        count = sub.get("count")
        if isinstance(et, str) and et in _PRIMITIVE_PY and isinstance(count, int):
            # Mirror the top-level uint8 → bytes special case.
            if et == "uint8":
                py_t, default = _render_pydantic_field(
                    "bytes",
                    default=f"bytes({count})",
                    constraints=[
                        f"min_length={count}", f"max_length={count}",
                    ],
                )
                return NestedField(name=name, py_type=py_t, default=default)
            inner = py_primitive_type(et)
            default_factory = (
                f"lambda: [{py_primitive_default(et)}] * {count}"
            )
            py_t, default = _render_pydantic_field(
                f"list[{inner}]",
                default_factory=default_factory,
                constraints=[f"min_length={count}", f"max_length={count}"],
            )
            return NestedField(name=name, py_type=py_t, default=default)

    raise NotImplementedError(
        f"struct sub-field {name!r} type {t!r} not supported"
    )


def _plan_top_field(
    f: dict[str, Any], nested_for_field: dict[str, str]
) -> TopField:
    """Plan a top-level message field. *nested_for_field* maps field
    name → nested class name when a struct element is involved."""
    t = f["type"]
    name = f["name"]

    if t in _PRIMITIVE_PY:
        py_t, default = _render_pydantic_field(
            py_primitive_type(t), default=py_primitive_default(t),
        )
        return TopField(name=name, py_type=py_t, default=default)

    if t == "fixed_string":
        py_t, default = _render_pydantic_field("str", default='""')
        return TopField(name=name, py_type=py_t, default=default)

    if t == "trailing_string":
        py_t, default = _render_pydantic_field("str", default='""')
        return TopField(name=name, py_type=py_t, default=default)

    if t == "length_prefixed_string":
        # Reference codec returns bytes for encoding="binary"
        # (STRING.value), str otherwise. We model both as str —
        # Pydantic accepts bytes-like for str fields with a
        # str_schema mode, but bytes is closer to truth here.
        # Use bytes for simplicity since STRING is the only
        # consumer and its caller usually wants bytes.
        if f.get("encoding") == "binary":
            py_t, default = _render_pydantic_field("bytes", default='b""')
        else:
            py_t, default = _render_pydantic_field("str", default='""')
        return TopField(name=name, py_type=py_t, default=default)

    if t == "fixed_bytes":
        py_t, default = _render_pydantic_field("bytes", default='b""')
        return TopField(name=name, py_type=py_t, default=default)

    if t == "array":
        et = f.get("element_type")
        count = f.get("count")
        count_from = f.get("count_from")

        if isinstance(et, str) and et in _PRIMITIVE_PY:
            # Special case: uint8 arrays are typed as `bytes` to
            # avoid materializing N Python int objects per element.
            # See codec/fields.py for the matching unpack change.
            if et == "uint8":
                if isinstance(count, int):
                    py_t, default = _render_pydantic_field(
                        "bytes",
                        default=f"bytes({count})",
                        constraints=[
                            f"min_length={count}", f"max_length={count}",
                        ],
                    )
                else:
                    py_t, default = _render_pydantic_field(
                        "bytes", default='b""',
                    )
                return TopField(name=name, py_type=py_t, default=default)

            inner = py_primitive_type(et)
            if isinstance(count, int):
                default_factory = (
                    f"lambda: [{py_primitive_default(et)}] * {count}"
                )
                py_t, default = _render_pydantic_field(
                    f"list[{inner}]",
                    default_factory=default_factory,
                    constraints=[f"min_length={count}", f"max_length={count}"],
                )
                return TopField(name=name, py_type=py_t, default=default)

            # Variable-count non-uint8 primitive. The codec returns
            # raw big-endian wire bytes; the typed layer promotes to
            # np.ndarray (with ``[numpy]`` extra) or array.array
            # (stdlib fallback). Declare as ``Any`` so Pydantic
            # accepts any of bytes/ndarray/array.array/list; a
            # validator normalizes them via coerce_variable_array.
            py_t = "Any"
            default = f"Field(default_factory=lambda: empty_variable_array({et!r}))"
            return TopField(
                name=name, py_type=py_t, default=default,
                variable_primitive_et=et,
            )

        if isinstance(et, dict) and et.get("type") == "fixed_string":
            py_t, default = _render_pydantic_field(
                "list[str]", default_factory="list",
            )
            return TopField(name=name, py_type=py_t, default=default)

        if isinstance(et, dict) and et.get("type") == "struct":
            nested_name = nested_for_field[name]
            py_t, default = _render_pydantic_field(
                f'list["{nested_name}"]',
                default_factory="list",
            )
            return TopField(name=name, py_type=py_t, default=default)

        raise NotImplementedError(
            f"array field {name!r} element_type {et!r} not supported"
        )

    raise NotImplementedError(f"field type {t!r} not supported")


# ---------------------------------------------------------------------------
# Whole-message plan
# ---------------------------------------------------------------------------


# Keys the codec field-walker actually consults. Documentation keys
# (description, rationale, layout, legacy_notes, ...) are stripped
# from the baked-in schema literal to keep generated modules small
# and reviewable.
_CODEC_RELEVANT_KEYS = {
    "name",
    "type",
    "element_type",
    "size_bytes",
    "count",
    "count_from",
    "encoding",
    "length_prefix_type",
    "null_padded",
    "null_terminated",
    "fields",  # struct element sub-fields
}


def _strip_doc_keys(value: Any) -> Any:
    """Recursively strip non-codec keys from a fields-list value."""
    if isinstance(value, dict):
        return {
            k: _strip_doc_keys(v)
            for k, v in value.items()
            if k in _CODEC_RELEVANT_KEYS
        }
    if isinstance(value, list):
        return [_strip_doc_keys(v) for v in value]
    return value


def plan_message(schema: dict[str, Any]) -> MessagePyPlan:
    type_id = schema["type_id"]
    class_name = py_class_name(type_id)
    fields_in = schema.get("fields", [])

    # Discover and disambiguate nested struct names. A name
    # collision with the message class itself gets "Entry" suffixed,
    # matching the C++ codegen rule.
    used = {class_name}
    nested_for_field: dict[str, str] = {}
    nested_classes: list[NestedClass] = []
    for f in fields_in:
        et = f.get("element_type")
        if isinstance(et, dict) and et.get("type") == "struct":
            base = nested_class_name(f["name"])
            nested_name = base
            while nested_name in used:
                nested_name = nested_name + "Entry"
            used.add(nested_name)
            nested_for_field[f["name"]] = nested_name
            nested_classes.append(NestedClass(
                name=nested_name,
                fields=[_plan_nested_field(sf) for sf in et["fields"]],
            ))

    top_fields = [_plan_top_field(f, nested_for_field) for f in fields_in]

    body_size = schema.get("body_size")
    is_fixed_body = isinstance(body_size, int)

    # Bake the schema fields list as a Python literal. Strip
    # documentation-only keys — they balloon the generated module
    # without affecting codec behaviour. Only keys that
    # `pack_fields` / `unpack_fields` actually read are preserved.
    # pprint.pformat produces a valid Python literal (True/False
    # not true/false; matters because the codec reads None/bool).
    schema_literal = pprint.pformat(
        _strip_doc_keys(fields_in), indent=4, width=78, sort_dicts=False
    )

    needs_arrays_runtime = any(
        f.variable_primitive_et is not None for f in top_fields
    )

    body_size_set = schema.get("body_size_set")
    if body_size_set is not None:
        body_size_set = sorted(int(x) for x in body_size_set)

    return MessagePyPlan(
        type_id=type_id,
        class_name=class_name,
        module_name=py_module_name(type_id),
        body_size=body_size if is_fixed_body else None,
        is_fixed_body=is_fixed_body,
        fields=top_fields,
        nested_classes=nested_classes,
        schema_literal=schema_literal,
        needs_arrays_runtime=needs_arrays_runtime,
        body_size_set=body_size_set,
    )
