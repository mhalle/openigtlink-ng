"""Schema-type → C++17-type mapping, plus per-field codegen metadata.

Each :class:`FieldPlan` is a flat, render-ready description of one
schema field; together with optional :class:`ElementStructDef`s for
struct-element nested types, they're enough for the Jinja templates
to stamp out a full ``.hpp/.cpp`` pair with no per-message logic.

Pack / unpack code conventions
------------------------------

Both pack and unpack carry a runtime cursor named ``off``
(``std::size_t``), declared in the template prelude and incremented
by each emitted code block.

- Top-level pack: reads from ``this`` (bare member name);
  writes into a pre-sized ``std::vector<std::uint8_t> out``.
- Top-level unpack: writes into a default-constructed ``out`` of
  the message type.
- Inside a struct element, both pack and unpack read/write through
  a local reference named ``elem``. The element-level planners
  emit ``elem.<sub>`` for both source and target.

Phase 4 supports
----------------

- Primitives (uint8..int64, float32, float64).
- Fixed-count primitive arrays.
- Sibling-count and ``count_from=remaining`` primitive arrays.
- ``fixed_string`` (null_padded=true and null_padded=false).
- ``trailing_string`` (null_terminated optional).
- ``length_prefixed_string`` (uint16 prefix).
- ``fixed_bytes``.
- Arrays of ``fixed_string`` elements (CAPABILITY, GET_BIND, STT_BIND).
- Arrays of ``struct`` elements with primitive / fixed_string /
  fixed-count primitive-array sub-fields (POINT, TDATA, TRAJ,
  IMGMETA, LBMETA, VIDEOMETA, BIND, POLYDATA, METADATA index).
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Optional


# ---------------------------------------------------------------------------
# Primitive type mapping
# ---------------------------------------------------------------------------

_PRIMITIVE_CXX: dict[str, tuple[str, int, str]] = {
    "uint8":   ("std::uint8_t",  1, "u8"),
    "uint16":  ("std::uint16_t", 2, "u16"),
    "uint32":  ("std::uint32_t", 4, "u32"),
    "uint64":  ("std::uint64_t", 8, "u64"),
    "int8":    ("std::int8_t",   1, "i8"),
    "int16":   ("std::int16_t",  2, "i16"),
    "int32":   ("std::int32_t",  4, "i32"),
    "int64":   ("std::int64_t",  8, "i64"),
    "float32": ("float",         4, "f32"),
    "float64": ("double",        8, "f64"),
}


def cxx_type_for_primitive(name: str) -> str:
    return _PRIMITIVE_CXX[name][0]


def primitive_byte_size(name: str) -> int:
    return _PRIMITIVE_CXX[name][1]


def primitive_be_suffix(name: str) -> str:
    return _PRIMITIVE_CXX[name][2]


_BO = "oigtl::runtime::byte_order"
_ERR = "oigtl::error"


# ---------------------------------------------------------------------------
# Naming helpers
# ---------------------------------------------------------------------------


def cxx_class_name(type_id: str) -> str:
    """Map ``"TRANSFORM"`` → ``"Transform"``, ``"STT_TRANSFORM"`` → ``"SttTransform"``."""
    return "".join(part.capitalize() for part in type_id.split("_"))


def header_basename(type_id: str) -> str:
    return type_id.lower()


def _singularize_word(word: str) -> str:
    """Naive English depluralization for nested-element type names.

    Handles only the patterns that appear in the OpenIGTLink schemas:
    ``-ies → -y``, ``-s → ``. Returns *word* unchanged if nothing
    matches.
    """
    if word.endswith("ies") and len(word) > 3:
        return word[:-3] + "y"
    if word.endswith("s") and not word.endswith("ss") and len(word) > 1:
        return word[:-1]
    return word


def element_struct_name(field_name: str) -> str:
    """Derive a nested-struct name from an array field name.

    ``points → Point``, ``header_entries → HeaderEntry``,
    ``type_ids → TypeId``. The output is always PascalCase.
    """
    parts = field_name.split("_")
    parts[-1] = _singularize_word(parts[-1])
    return "".join(p.capitalize() for p in parts)


# ---------------------------------------------------------------------------
# Render-ready dataclasses
# ---------------------------------------------------------------------------


@dataclass
class ElementMember:
    """One sub-field declaration inside a nested element struct."""
    name: str
    cxx_type: str
    init: str


@dataclass
class ElementStructDef:
    """A nested struct emitted inside a message class.

    Carries everything the template needs to stamp out a struct
    definition: members, plus the per-element pack/unpack code that
    operates on a local reference named ``elem``.
    """
    name: str                       # e.g. "Point"
    members: list[ElementMember]
    element_size: int               # statically known total wire bytes
    pack_lines: list[str]           # operate on `elem.<member>`
    unpack_lines: list[str]


@dataclass
class FieldPlan:
    """One C++ field, ready for the template."""

    name: str
    cxx_type: str
    init: str = "{}"
    static_byte_size: Optional[int] = 0
    pack_size_expr: str = "0"
    pack_lines: list[str] = field(default_factory=list)
    unpack_lines: list[str] = field(default_factory=list)
    # Optional nested element struct emitted for this field
    # (array<struct> only).
    element_struct: Optional[ElementStructDef] = None


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _short_buffer_check(name: str, need_expr: str) -> str:
    return (
        f'if (off + ({need_expr}) > length) {{ '
        f'throw {_ERR}::ShortBufferError("{name}: short buffer"); }}'
    )


# ---------------------------------------------------------------------------
# Top-level field planners (read from `name`, write to `out.name`)
# ---------------------------------------------------------------------------


def _plan_primitive(name: str, ptype: str) -> FieldPlan:
    cxx = cxx_type_for_primitive(ptype)
    suf = primitive_be_suffix(ptype)
    size = primitive_byte_size(ptype)
    plan = FieldPlan(
        name=name, cxx_type=cxx, init="{}",
        static_byte_size=size, pack_size_expr=str(size),
    )
    plan.pack_lines += [
        f"{_BO}::write_be_{suf}(out.data() + off, {name});",
        f"off += {size};",
    ]
    plan.unpack_lines += [
        _short_buffer_check(name, str(size)),
        f"out.{name} = {_BO}::read_be_{suf}(data + off);",
        f"off += {size};",
    ]
    return plan


def _plan_array_fixed(name: str, etype: str, count: int) -> FieldPlan:
    elem_cxx = cxx_type_for_primitive(etype)
    elem_size = primitive_byte_size(etype)
    suf = primitive_be_suffix(etype)
    total = elem_size * count
    plan = FieldPlan(
        name=name,
        cxx_type=f"std::array<{elem_cxx}, {count}>",
        init="{}",
        static_byte_size=total,
        pack_size_expr=str(total),
    )
    plan.pack_lines += [
        f"for (std::size_t i = 0; i < {count}; ++i) {{",
        f"    {_BO}::write_be_{suf}(out.data() + off + i * {elem_size}, "
        f"{name}[i]);",
        "}",
        f"off += {total};",
    ]
    plan.unpack_lines += [
        _short_buffer_check(name, str(total)),
        f"for (std::size_t i = 0; i < {count}; ++i) {{",
        f"    out.{name}[i] = {_BO}::read_be_{suf}(data + off + i * {elem_size});",
        "}",
        f"off += {total};",
    ]
    return plan


def _plan_array_sibling(name: str, etype: str, sibling: str) -> FieldPlan:
    elem_cxx = cxx_type_for_primitive(etype)
    elem_size = primitive_byte_size(etype)
    suf = primitive_be_suffix(etype)
    plan = FieldPlan(
        name=name, cxx_type=f"std::vector<{elem_cxx}>", init="",
        static_byte_size=None,
        pack_size_expr=f"{name}.size() * {elem_size}",
    )
    plan.pack_lines += [
        f"for (std::size_t i = 0; i < {name}.size(); ++i) {{",
        f"    {_BO}::write_be_{suf}(out.data() + off + i * {elem_size}, "
        f"{name}[i]);",
        "}",
        f"off += {name}.size() * {elem_size};",
    ]
    plan.unpack_lines += [
        "{",
        f"    std::size_t count = static_cast<std::size_t>(out.{sibling});",
        f"    {_short_buffer_check(name, f'count * {elem_size}')}",
        f"    out.{name}.resize(count);",
        "    for (std::size_t i = 0; i < count; ++i) {",
        f"        out.{name}[i] = {_BO}::read_be_{suf}(data + off + i * {elem_size});",
        "    }",
        f"    off += count * {elem_size};",
        "}",
    ]
    return plan


def _plan_array_remaining(name: str, etype: str) -> FieldPlan:
    elem_cxx = cxx_type_for_primitive(etype)
    elem_size = primitive_byte_size(etype)
    suf = primitive_be_suffix(etype)
    plan = FieldPlan(
        name=name, cxx_type=f"std::vector<{elem_cxx}>", init="",
        static_byte_size=None,
        pack_size_expr=f"{name}.size() * {elem_size}",
    )
    plan.pack_lines += [
        f"for (std::size_t i = 0; i < {name}.size(); ++i) {{",
        f"    {_BO}::write_be_{suf}(out.data() + off + i * {elem_size}, "
        f"{name}[i]);",
        "}",
        f"off += {name}.size() * {elem_size};",
    ]
    plan.unpack_lines += [
        "{",
        "    std::size_t bytes = length - off;",
        f"    if (bytes % {elem_size} != 0) {{ "
        f'throw {_ERR}::MalformedMessageError("{name}: trailing '
        f'bytes not divisible by element size"); }}',
        f"    std::size_t count = bytes / {elem_size};",
        f"    out.{name}.resize(count);",
        "    for (std::size_t i = 0; i < count; ++i) {",
        f"        out.{name}[i] = {_BO}::read_be_{suf}(data + off + i * {elem_size});",
        "    }",
        "    off += bytes;",
        "}",
    ]
    return plan


def _plan_fixed_string(name: str, size: int, null_padded: bool) -> FieldPlan:
    plan = FieldPlan(
        name=name, cxx_type="std::string", init="",
        static_byte_size=size, pack_size_expr=str(size),
    )
    plan.pack_lines += [
        "{",
        f"    constexpr std::size_t n = {size};",
        f"    std::size_t copy_len = {name}.size() < n ? {name}.size() : n;",
        f"    std::memcpy(out.data() + off, {name}.data(), copy_len);",
        "    if (copy_len < n) {",
        "        std::memset(out.data() + off + copy_len, 0, n - copy_len);",
        "    }",
        f"    off += {size};",
        "}",
    ]
    if null_padded:
        plan.unpack_lines += [
            _short_buffer_check(name, str(size)),
            "{",
            f"    constexpr std::size_t n = {size};",
            f'    const std::size_t len = oigtl::runtime::ascii::'
            f'null_padded_length(data + off, n, "{name}");',
            f"    out.{name}.assign("
            "reinterpret_cast<const char*>(data + off), len);",
            f"    off += {size};",
            "}",
        ]
    else:
        # null_padded=false: take the full N bytes verbatim, no NUL strip.
        # Used by VIDEO/STT_VIDEO codec field where the fixed-width
        # string is meaningful through its full length. Every byte
        # must still be ASCII.
        plan.unpack_lines += [
            _short_buffer_check(name, str(size)),
            f'oigtl::runtime::ascii::check_bytes('
            f'data + off, {size}, "{name}");',
            f"out.{name}.assign("
            f"reinterpret_cast<const char*>(data + off), {size});",
            f"off += {size};",
        ]
    return plan


def _plan_trailing_string(
    name: str, null_terminated: bool, encoding: str = "ascii"
) -> FieldPlan:
    plan = FieldPlan(
        name=name, cxx_type="std::string", init="",
        static_byte_size=None,
        pack_size_expr=(
            f"{name}.size() + 1" if null_terminated else f"{name}.size()"
        ),
    )
    plan.pack_lines += [
        f"std::memcpy(out.data() + off, {name}.data(), {name}.size());",
        f"off += {name}.size();",
    ]
    ascii_check = encoding == "ascii"
    if null_terminated:
        plan.pack_lines += ["out[off] = 0;", "off += 1;"]
        unpack_lines = [
            "{",
            "    std::size_t end = length;",
            "    if (end > off && data[end - 1] == 0) { --end; }",
        ]
        if ascii_check:
            unpack_lines += [
                f'    oigtl::runtime::ascii::check_bytes('
                f'data + off, end - off, "{name}");',
            ]
        unpack_lines += [
            f"    out.{name}.assign("
            "reinterpret_cast<const char*>(data + off), end - off);",
            "    off = length;",
            "}",
        ]
        plan.unpack_lines += unpack_lines
    else:
        if ascii_check:
            plan.unpack_lines += [
                f'oigtl::runtime::ascii::check_bytes('
                f'data + off, length - off, "{name}");',
            ]
        plan.unpack_lines += [
            f"out.{name}.assign("
            f"reinterpret_cast<const char*>(data + off), length - off);",
            "off = length;",
        ]
    return plan


def _plan_length_prefixed_string(
    name: str, prefix_type: str, encoding: str = "ascii"
) -> FieldPlan:
    if prefix_type != "uint16":
        raise NotImplementedError(
            f"length_prefixed_string with length_prefix_type={prefix_type!r} "
            "not yet supported"
        )
    plan = FieldPlan(
        name=name, cxx_type="std::string", init="",
        static_byte_size=None, pack_size_expr=f"2 + {name}.size()",
    )
    plan.pack_lines += [
        f"{_BO}::write_be_u16(out.data() + off, "
        f"static_cast<std::uint16_t>({name}.size()));",
        "off += 2;",
        f"std::memcpy(out.data() + off, {name}.data(), {name}.size());",
        f"off += {name}.size();",
    ]
    plan.unpack_lines += [
        _short_buffer_check(name, "2"),
        f"std::uint16_t {name}__len = {_BO}::read_be_u16(data + off);",
        "off += 2;",
        _short_buffer_check(name, f"{name}__len"),
    ]
    if encoding == "ascii":
        plan.unpack_lines += [
            f'oigtl::runtime::ascii::check_bytes('
            f'data + off, {name}__len, "{name}");',
        ]
    plan.unpack_lines += [
        f"out.{name}.assign("
        f"reinterpret_cast<const char*>(data + off), {name}__len);",
        f"off += {name}__len;",
    ]
    return plan


def _plan_fixed_bytes(name: str, size: int) -> FieldPlan:
    plan = FieldPlan(
        name=name, cxx_type=f"std::array<std::uint8_t, {size}>",
        init="{}", static_byte_size=size, pack_size_expr=str(size),
    )
    plan.pack_lines += [
        f"std::memcpy(out.data() + off, {name}.data(), {size});",
        f"off += {size};",
    ]
    plan.unpack_lines += [
        _short_buffer_check(name, str(size)),
        f"std::memcpy(out.{name}.data(), data + off, {size});",
        f"off += {size};",
    ]
    return plan


# ---------------------------------------------------------------------------
# Element sub-field planners (pack/unpack via `elem.<sub>`)
#
# These cover the leaf types that can appear inside a struct element.
# All are statically sized — struct elements with variable-length
# sub-fields are not specified anywhere in the OpenIGTLink schemas
# and would complicate `count_from=remaining` for arrays of them.
# ---------------------------------------------------------------------------


@dataclass
class _SubFieldPlan:
    """One sub-field inside a struct element."""
    name: str
    cxx_type: str
    init: str
    byte_size: int
    pack_lines: list[str]   # operate on elem.<name>
    unpack_lines: list[str]


def _plan_sub_primitive(name: str, ptype: str) -> _SubFieldPlan:
    suf = primitive_be_suffix(ptype)
    size = primitive_byte_size(ptype)
    return _SubFieldPlan(
        name=name, cxx_type=cxx_type_for_primitive(ptype),
        init="{}", byte_size=size,
        pack_lines=[
            f"{_BO}::write_be_{suf}(out.data() + off, elem.{name});",
            f"off += {size};",
        ],
        unpack_lines=[
            f"elem.{name} = {_BO}::read_be_{suf}(data + off);",
            f"off += {size};",
        ],
    )


def _plan_sub_fixed_string(
    name: str, size: int, null_padded: bool
) -> _SubFieldPlan:
    pack = [
        "{",
        f"    constexpr std::size_t n = {size};",
        f"    std::size_t copy_len = elem.{name}.size() < n "
        f"? elem.{name}.size() : n;",
        f"    std::memcpy(out.data() + off, elem.{name}.data(), copy_len);",
        "    if (copy_len < n) {",
        "        std::memset(out.data() + off + copy_len, 0, n - copy_len);",
        "    }",
        f"    off += {size};",
        "}",
    ]
    if null_padded:
        unpack = [
            "{",
            f"    constexpr std::size_t n = {size};",
            f'    const std::size_t len = oigtl::runtime::ascii::'
            f'null_padded_length(data + off, n, "{name}");',
            f"    elem.{name}.assign("
            "reinterpret_cast<const char*>(data + off), len);",
            f"    off += {size};",
            "}",
        ]
    else:
        unpack = [
            f'oigtl::runtime::ascii::check_bytes('
            f'data + off, {size}, "{name}");',
            f"elem.{name}.assign("
            f"reinterpret_cast<const char*>(data + off), {size});",
            f"off += {size};",
        ]
    return _SubFieldPlan(
        name=name, cxx_type="std::string", init="",
        byte_size=size, pack_lines=pack, unpack_lines=unpack,
    )


def _plan_sub_array_fixed(
    name: str, etype: str, count: int
) -> _SubFieldPlan:
    elem_cxx = cxx_type_for_primitive(etype)
    elem_size = primitive_byte_size(etype)
    suf = primitive_be_suffix(etype)
    total = elem_size * count
    return _SubFieldPlan(
        name=name, cxx_type=f"std::array<{elem_cxx}, {count}>",
        init="{}", byte_size=total,
        pack_lines=[
            f"for (std::size_t i = 0; i < {count}; ++i) {{",
            f"    {_BO}::write_be_{suf}(out.data() + off + i * {elem_size}, "
            f"elem.{name}[i]);",
            "}",
            f"off += {total};",
        ],
        unpack_lines=[
            f"for (std::size_t i = 0; i < {count}; ++i) {{",
            f"    elem.{name}[i] = {_BO}::read_be_{suf}("
            f"data + off + i * {elem_size});",
            "}",
            f"off += {total};",
        ],
    )


def _plan_sub_field(f: dict[str, Any]) -> _SubFieldPlan:
    """Plan one sub-field inside a struct element."""
    t = f["type"]
    name = f["name"]
    if t in _PRIMITIVE_CXX:
        return _plan_sub_primitive(name, t)
    if t == "fixed_string":
        return _plan_sub_fixed_string(
            name, int(f["size_bytes"]), bool(f.get("null_padded", True))
        )
    if t == "array":
        etype = f.get("element_type")
        count = f.get("count")
        if (
            isinstance(etype, str) and etype in _PRIMITIVE_CXX
            and isinstance(count, int)
        ):
            return _plan_sub_array_fixed(name, etype, count)
    raise NotImplementedError(
        f"struct sub-field {name!r} type {t!r} not supported "
        "(struct elements only allow fixed-size sub-fields)"
    )


def _build_element_struct(
    field_name: str, elem_def: dict[str, Any]
) -> ElementStructDef:
    """Build an ElementStructDef from a struct element_type."""
    sub_plans = [_plan_sub_field(sf) for sf in elem_def["fields"]]
    return ElementStructDef(
        name=element_struct_name(field_name),
        members=[
            ElementMember(name=p.name, cxx_type=p.cxx_type, init=p.init)
            for p in sub_plans
        ],
        element_size=sum(p.byte_size for p in sub_plans),
        pack_lines=[ln for p in sub_plans for ln in p.pack_lines],
        unpack_lines=[ln for p in sub_plans for ln in p.unpack_lines],
    )


# ---------------------------------------------------------------------------
# Array<struct> planners — one per count strategy
# ---------------------------------------------------------------------------


def _emit_struct_pack_loop(
    name: str, elem_struct: ElementStructDef
) -> list[str]:
    return [
        f"for (std::size_t i = 0; i < {name}.size(); ++i) {{",
        f"    const auto& elem = {name}[i];",
        *[f"    {ln}" for ln in elem_struct.pack_lines],
        "}",
    ]


def _emit_struct_unpack_loop(
    name: str, elem_struct: ElementStructDef, count_expr: str
) -> list[str]:
    """Unpack `count_expr` elements into out.<name>."""
    return [
        f"out.{name}.resize({count_expr});",
        f"for (std::size_t i = 0; i < {count_expr}; ++i) {{",
        f"    auto& elem = out.{name}[i];",
        *[f"    {ln}" for ln in elem_struct.unpack_lines],
        "}",
    ]


def _plan_array_struct(
    name: str,
    elem_def: dict[str, Any],
    count: Any,
    count_from: Optional[str],
) -> FieldPlan:
    elem_struct = _build_element_struct(name, elem_def)
    cxx_elem = elem_struct.name
    plan = FieldPlan(
        name=name, cxx_type=f"std::vector<{cxx_elem}>", init="",
        static_byte_size=None,
        pack_size_expr=f"{name}.size() * {elem_struct.element_size}",
        element_struct=elem_struct,
    )
    plan.pack_lines += _emit_struct_pack_loop(name, elem_struct)
    plan.pack_lines.append(
        f"// (off advanced inside loop)"
    )

    if isinstance(count, int):
        count_expr = str(count)
        guard = _short_buffer_check(
            name, f"{count_expr} * {elem_struct.element_size}"
        )
        plan.unpack_lines += [
            "{",
            f"    {guard}",
            *[f"    {ln}" for ln in
              _emit_struct_unpack_loop(name, elem_struct, count_expr)],
            "}",
        ]
    elif isinstance(count, str):
        plan.unpack_lines += [
            "{",
            f"    std::size_t count = static_cast<std::size_t>(out.{count});",
            f"    {_short_buffer_check(name, f'count * {elem_struct.element_size}')}",
            *[f"    {ln}" for ln in
              _emit_struct_unpack_loop(name, elem_struct, "count")],
            "}",
        ]
    elif count_from == "remaining":
        sz = elem_struct.element_size
        plan.unpack_lines += [
            "{",
            "    std::size_t bytes = length - off;",
            f"    if (bytes % {sz} != 0) {{ "
            f'throw {_ERR}::MalformedMessageError("{name}: trailing '
            f'bytes not divisible by element size"); }}',
            f"    std::size_t count = bytes / {sz};",
            *[f"    {ln}" for ln in
              _emit_struct_unpack_loop(name, elem_struct, "count")],
            "}",
        ]
    else:
        raise NotImplementedError(
            f"array<struct> field {name!r}: no count, sibling count, "
            "or count_from=remaining"
        )
    return plan


# ---------------------------------------------------------------------------
# Array<fixed_string> planners
# ---------------------------------------------------------------------------


def _plan_array_fixed_string(
    name: str,
    elem_def: dict[str, Any],
    count: Any,
    count_from: Optional[str],
) -> FieldPlan:
    size = int(elem_def["size_bytes"])
    null_padded = bool(elem_def.get("null_padded", True))
    plan = FieldPlan(
        name=name, cxx_type="std::vector<std::string>", init="",
        static_byte_size=None, pack_size_expr=f"{name}.size() * {size}",
    )
    pack_loop = [
        f"for (std::size_t i = 0; i < {name}.size(); ++i) {{",
        "    {",
        f"        constexpr std::size_t n = {size};",
        f"        std::size_t copy_len = {name}[i].size() < n "
        f"? {name}[i].size() : n;",
        f"        std::memcpy(out.data() + off, {name}[i].data(), copy_len);",
        "        if (copy_len < n) {",
        "            std::memset(out.data() + off + copy_len, 0, n - copy_len);",
        "        }",
        f"        off += {size};",
        "    }",
        "}",
    ]
    plan.pack_lines += pack_loop

    if null_padded:
        unpack_one = [
            f"        constexpr std::size_t n = {size};",
            "        std::size_t len = 0;",
            "        while (len < n && data[off + len] != 0) { ++len; }",
            f"        out.{name}[i].assign("
            "reinterpret_cast<const char*>(data + off), len);",
            f"        off += {size};",
        ]
    else:
        unpack_one = [
            f"        out.{name}[i].assign("
            f"reinterpret_cast<const char*>(data + off), {size});",
            f"        off += {size};",
        ]

    if isinstance(count, int):
        count_expr = str(count)
    elif isinstance(count, str):
        count_expr = f"static_cast<std::size_t>(out.{count})"
    elif count_from == "remaining":
        count_expr = f"(length - off) / {size}"
    else:
        raise NotImplementedError(
            f"array<fixed_string> field {name!r}: no count strategy"
        )

    plan.unpack_lines += [
        "{",
        f"    std::size_t count = {count_expr};",
        f"    {_short_buffer_check(name, f'count * {size}')}",
        f"    out.{name}.resize(count);",
        "    for (std::size_t i = 0; i < count; ++i) {",
        "        {",
        *unpack_one,
        "        }",
        "    }",
        "}",
    ]
    return plan


# ---------------------------------------------------------------------------
# plan_field — the dispatch table
# ---------------------------------------------------------------------------


def plan_field(f: dict[str, Any]) -> FieldPlan:
    """Build a :class:`FieldPlan` for the schema field *f*.

    Raises :class:`NotImplementedError` for shapes the current codegen
    does not yet support.
    """
    t = f["type"]
    name = f["name"]

    if t in _PRIMITIVE_CXX:
        return _plan_primitive(name, t)

    if t == "fixed_string":
        return _plan_fixed_string(
            name, int(f["size_bytes"]), bool(f.get("null_padded", True)),
        )

    if t == "trailing_string":
        return _plan_trailing_string(
            name,
            bool(f.get("null_terminated", False)),
            encoding=f.get("encoding", "ascii"),
        )

    if t == "length_prefixed_string":
        return _plan_length_prefixed_string(
            name, f["length_prefix_type"],
            encoding=f.get("encoding", "ascii"),
        )

    if t == "fixed_bytes":
        return _plan_fixed_bytes(name, int(f["size_bytes"]))

    if t == "array":
        etype = f.get("element_type")
        count = f.get("count")
        count_from = f.get("count_from")

        if isinstance(etype, str) and etype in _PRIMITIVE_CXX:
            if isinstance(count, int):
                return _plan_array_fixed(name, etype, count)
            if isinstance(count, str):
                return _plan_array_sibling(name, etype, count)
            if count_from == "remaining":
                return _plan_array_remaining(name, etype)
            raise NotImplementedError(
                f"array field {name!r} has no count, sibling count, "
                "or count_from=remaining"
            )

        if isinstance(etype, dict):
            ekind = etype.get("type")
            if ekind == "struct":
                return _plan_array_struct(name, etype, count, count_from)
            if ekind == "fixed_string":
                return _plan_array_fixed_string(name, etype, count, count_from)
            raise NotImplementedError(
                f"array field {name!r}: element_type kind {ekind!r} "
                "not yet supported"
            )

        raise NotImplementedError(
            f"array field {name!r}: element_type {etype!r} not yet supported"
        )

    raise NotImplementedError(f"field type {t!r} not yet supported")


# ---------------------------------------------------------------------------
# Whole-message plan
# ---------------------------------------------------------------------------


@dataclass
class MessagePlan:
    type_id: str
    class_name: str
    header_basename: str
    is_fixed_body: bool
    body_size: Optional[int]
    body_size_expr: str
    fields: list[FieldPlan]
    nested_structs: list[ElementStructDef]
    body_size_set: Optional[list[int]] = None


def plan_message(schema: dict[str, Any]) -> MessagePlan:
    type_id = schema["type_id"]
    fields_in = schema.get("fields", [])
    field_plans: list[FieldPlan] = [plan_field(f) for f in fields_in]

    nested_structs: list[ElementStructDef] = [
        p.element_struct for p in field_plans if p.element_struct is not None
    ]

    # Resolve nested-struct name collisions. Two failure modes occur
    # in practice:
    # 1. The singularized field name collides with the message class
    #    name itself (POINT.points → 'Point' inside class 'Point').
    # 2. Two struct fields singularize to the same name (no current
    #    schema does this, but cheap to guard).
    # In both cases we append "Entry" until the name is unique. Also
    # rewrites the corresponding FieldPlan.cxx_type so the
    # std::vector<...> declaration matches.
    class_name = cxx_class_name(type_id)
    used = {class_name}
    for plan in field_plans:
        es = plan.element_struct
        if es is None:
            continue
        new_name = es.name
        while new_name in used:
            new_name = new_name + "Entry"
        if new_name != es.name:
            old_cxx = plan.cxx_type
            plan.cxx_type = old_cxx.replace(f"<{es.name}>", f"<{new_name}>")
            es.name = new_name
        used.add(new_name)

    if all(p.static_byte_size is not None for p in field_plans):
        body_size: Optional[int] = sum(
            p.static_byte_size for p in field_plans  # type: ignore[misc]
        )
        body_size_expr = str(body_size)
        is_fixed_body = True

        declared = schema.get("body_size")
        if isinstance(declared, int) and declared != body_size:
            raise ValueError(
                f"schema {type_id}: declared body_size={declared} but "
                f"field plans sum to {body_size} bytes"
            )
    else:
        body_size = None
        body_size_expr = (
            " + ".join(f"({p.pack_size_expr})" for p in field_plans)
            if field_plans else "0"
        )
        is_fixed_body = False

    body_size_set = schema.get("body_size_set")
    if body_size_set is not None:
        body_size_set = sorted(int(x) for x in body_size_set)

    return MessagePlan(
        type_id=type_id,
        class_name=cxx_class_name(type_id),
        header_basename=header_basename(type_id),
        is_fixed_body=is_fixed_body,
        body_size=body_size,
        body_size_expr=body_size_expr,
        fields=field_plans,
        nested_structs=nested_structs,
        body_size_set=body_size_set,
    )
