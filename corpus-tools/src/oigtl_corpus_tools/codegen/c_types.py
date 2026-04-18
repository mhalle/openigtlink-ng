"""Schema → C-struct / C-function mapping for the core-c target.

This is a sibling to :mod:`cpp_types`, tuned for C99 embedded consumers:

* No C++ templates, no `std::vector`, no `std::string`, no heap.
* Variable-length wire fields become ``(const uint8_t *ptr,
  size_t len)`` views into the caller's input buffer (for
  trailing strings: ``const char *``). The caller's application
  copies into owned storage if it wants to persist the value
  past the wire buffer's lifetime.
* Pack / unpack live in free functions taking an explicit
  ``oigtl_<type>_t *`` and a caller-supplied byte buffer.

Supported field shapes (round 1):

* Primitives (u8/i8/u16/i16/u32/i32/u64/i64/f32/f64).
* Fixed-count primitive arrays.
* Sibling-count and ``count_from=remaining`` variable primitive
  arrays (view-based).
* Null-padded fixed strings (embedded as ``char[N+1]`` in the
  struct; null-terminated after unpack).
* Null-terminated and raw trailing strings (view-based).

Unsupported shapes (raise ``NotImplementedError`` → caller skips):

* Length-prefixed strings (rare; add if a user needs it).
* Fixed-string-element arrays (CAPABILITY etc.).
* Struct-element arrays (POINT / TDATA / POLYDATA / …).
* Metadata (v2+).
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Optional


# ---------------------------------------------------------------------------
# Primitive type mapping
# ---------------------------------------------------------------------------

# C type, byte width, big-endian helper suffix (matching byte_order.h).
_PRIMITIVE_C: dict[str, tuple[str, int, str]] = {
    "uint8":   ("uint8_t",  1, "u8"),
    "uint16":  ("uint16_t", 2, "u16"),
    "uint32":  ("uint32_t", 4, "u32"),
    "uint64":  ("uint64_t", 8, "u64"),
    "int8":    ("int8_t",   1, "i8"),
    "int16":   ("int16_t",  2, "i16"),
    "int32":   ("int32_t",  4, "i32"),
    "int64":   ("int64_t",  8, "i64"),
    "float32": ("float",    4, "f32"),
    "float64": ("double",   8, "f64"),
}


def c_primitive_type(name: str) -> str:
    return _PRIMITIVE_C[name][0]


def c_primitive_size(name: str) -> int:
    return _PRIMITIVE_C[name][1]


def c_primitive_suffix(name: str) -> str:
    return _PRIMITIVE_C[name][2]


# ---------------------------------------------------------------------------
# Naming
# ---------------------------------------------------------------------------


def c_type_struct(type_id: str) -> str:
    """TRANSFORM → ``oigtl_transform_t``. STT_TDATA → ``oigtl_stt_tdata_t``."""
    return f"oigtl_{type_id.lower()}_t"


def c_type_prefix(type_id: str) -> str:
    """Function-name prefix. TRANSFORM → ``oigtl_transform``."""
    return f"oigtl_{type_id.lower()}"


def c_body_size_macro(type_id: str) -> str:
    """Constant-size macro. TRANSFORM → ``OIGTL_TRANSFORM_BODY_SIZE``."""
    return f"OIGTL_{type_id.upper()}_BODY_SIZE"


def c_header_basename(type_id: str) -> str:
    """File stem. TRANSFORM → ``transform``."""
    return type_id.lower()


# ---------------------------------------------------------------------------
# Field plans — flat render-ready per-field data the templates consume.
# ---------------------------------------------------------------------------


@dataclass
class FieldPlan:
    """One C field, decorated with the lines the templates stamp out.

    Line lists are written against the conventions:

    * ``pack_lines``: read from ``msg->field``, write at
      ``buf + off``. Advance ``off`` as needed.
    * ``unpack_lines``: read from ``buf + off``, write to
      ``out->field``. Advance ``off`` as needed. May check
      ``buf_len`` for short-buffer errors.
    * ``size_expr``: an expression — constant literal when the
      field's wire width is known, otherwise a C expression that
      reads from ``msg->...`` at runtime. Used to size the body
      in pack().

    ``static_size`` is the known compile-time size in bytes,
    or ``None`` for variable-width fields (views).
    """

    name: str
    declaration: str              # e.g. "float matrix[12];"
    static_size: Optional[int]    # None for variable-width
    size_expr: str                # literal int string, or "msg->foo_bytes"
    pack_lines: list[str] = field(default_factory=list)
    unpack_lines: list[str] = field(default_factory=list)
    # Doc-comment lines emitted above the struct member declaration.
    doc: list[str] = field(default_factory=list)


@dataclass
class MessagePlan:
    """Everything one Jinja render of a message needs."""

    type_id: str                         # "TRANSFORM"
    struct_name: str                     # "oigtl_transform_t"
    fn_prefix: str                       # "oigtl_transform"
    body_size_macro: str                 # "OIGTL_TRANSFORM_BODY_SIZE"
    basename: str                        # "transform"
    is_fixed_body: bool
    body_size: Optional[int]             # int if fixed, else None
    body_size_expr: str                  # literal "48" or a C expr
    body_size_set: Optional[list[int]]   # e.g. [12, 24, 28] for POSITION
    fields: list[FieldPlan]
    description: str                     # schema "description" field


# ---------------------------------------------------------------------------
# Per-field planners
# ---------------------------------------------------------------------------


def _plan_primitive(name: str, ptype: str) -> FieldPlan:
    ct = c_primitive_type(ptype)
    size = c_primitive_size(ptype)
    suf = c_primitive_suffix(ptype)
    p = FieldPlan(
        name=name,
        declaration=f"{ct} {name};",
        static_size=size,
        size_expr=str(size),
    )
    p.pack_lines += [
        f"oigtl_write_be_{suf}(buf + off, msg->{name});",
        f"off += {size};",
    ]
    p.unpack_lines += [
        f"if (off + {size} > len) return OIGTL_ERR_SHORT_BUFFER;",
        f"out->{name} = oigtl_read_be_{suf}(buf + off);",
        f"off += {size};",
    ]
    return p


def _plan_array_fixed(name: str, etype: str, count: int) -> FieldPlan:
    ct = c_primitive_type(etype)
    esize = c_primitive_size(etype)
    suf = c_primitive_suffix(etype)
    total = esize * count
    p = FieldPlan(
        name=name,
        declaration=f"{ct} {name}[{count}];",
        static_size=total,
        size_expr=str(total),
    )
    p.pack_lines += [
        f"for (size_t i = 0; i < {count}; ++i) {{",
        f"    oigtl_write_be_{suf}(buf + off + i * {esize}, "
        f"msg->{name}[i]);",
        "}",
        f"off += {total};",
    ]
    p.unpack_lines += [
        f"if (off + {total} > len) return OIGTL_ERR_SHORT_BUFFER;",
        f"for (size_t i = 0; i < {count}; ++i) {{",
        f"    out->{name}[i] = oigtl_read_be_{suf}(buf + off + i * {esize});",
        "}",
        f"off += {total};",
    ]
    return p


def _plan_array_sibling(name: str, etype: str, sibling: str) -> FieldPlan:
    """Variable-count primitive array whose count lives in another field.

    View pattern: pack reads from ``msg->{name}`` + ``msg->{name}_bytes``;
    unpack sets ``out->{name}`` to a pointer into the wire buffer and
    ``out->{name}_bytes`` to the byte count. No allocation.
    """
    esize = c_primitive_size(etype)
    p = FieldPlan(
        name=name,
        declaration=(
            f"/* view: points into wire bytes — see README for lifetime */\n"
            f"    const uint8_t *{name};\n"
            f"    size_t         {name}_bytes;"
        ),
        static_size=None,
        size_expr=f"msg->{name}_bytes",
    )
    p.pack_lines += [
        f"if (msg->{name}_bytes > 0) {{",
        f"    memcpy(buf + off, msg->{name}, msg->{name}_bytes);",
        f"    off += msg->{name}_bytes;",
        "}",
    ]
    p.unpack_lines += [
        "{",
        f"    size_t count = (size_t)out->{sibling};",
        f"    size_t bytes = count * {esize};",
        f"    if (off + bytes > len) return OIGTL_ERR_SHORT_BUFFER;",
        f"    out->{name} = buf + off;",
        f"    out->{name}_bytes = bytes;",
        "    off += bytes;",
        "}",
    ]
    return p


def _plan_array_remaining(name: str, etype: str) -> FieldPlan:
    """Variable primitive array that consumes the remainder of the body.

    Same view pattern as `_plan_array_sibling`; the count is inferred
    from ``len - off`` and must be a multiple of the element size.
    """
    esize = c_primitive_size(etype)
    p = FieldPlan(
        name=name,
        declaration=(
            f"/* view: points into wire bytes — see README for lifetime */\n"
            f"    const uint8_t *{name};\n"
            f"    size_t         {name}_bytes;"
        ),
        static_size=None,
        size_expr=f"msg->{name}_bytes",
    )
    p.pack_lines += [
        f"if (msg->{name}_bytes > 0) {{",
        f"    memcpy(buf + off, msg->{name}, msg->{name}_bytes);",
        f"    off += msg->{name}_bytes;",
        "}",
    ]
    p.unpack_lines += [
        "{",
        "    size_t bytes = len - off;",
        f"    if (bytes % {esize} != 0) return OIGTL_ERR_MALFORMED;",
        f"    out->{name} = buf + off;",
        f"    out->{name}_bytes = bytes;",
        "    off += bytes;",
        "}",
    ]
    return p


def _plan_fixed_string_null_padded(name: str, size: int) -> FieldPlan:
    """Fixed-width ASCII string, null-padded to ``size`` on the wire.

    Embedded as ``char[size + 1]`` in the struct so the unpacked
    value is always a valid C string. Pack validates the input
    does not exceed ``size`` bytes.
    """
    p = FieldPlan(
        name=name,
        declaration=f"char {name}[{size + 1}];",
        static_size=size,
        size_expr=str(size),
    )
    p.pack_lines += [
        "{",
        f"    size_t n = strlen(msg->{name});",
        f"    if (n > {size}) return OIGTL_ERR_FIELD_RANGE;",
        f"    memcpy(buf + off, msg->{name}, n);",
        f"    if (n < {size}) memset(buf + off + n, 0, {size} - n);",
        f"    off += {size};",
        "}",
    ]
    p.unpack_lines += [
        f"if (off + {size} > len) return OIGTL_ERR_SHORT_BUFFER;",
        "{",
        f"    int n = oigtl_null_padded_length(buf + off, {size});",
        "    if (n < 0) return n;",
        f"    memcpy(out->{name}, buf + off, (size_t)n);",
        f"    out->{name}[n] = '\\0';",
        f"    off += {size};",
        "}",
    ]
    return p


def _plan_fixed_string_raw(name: str, size: int) -> FieldPlan:
    """Fixed-width string, NOT null-padded. Carries ``size`` bytes
    of payload verbatim — used by VIDEO's 4-byte ``codec`` FourCC
    field. Embedded as ``char[size + 1]`` in the struct; the extra
    byte lets us always produce a valid C string even though the
    wire doesn't terminate.
    """
    p = FieldPlan(
        name=name,
        declaration=f"char {name}[{size + 1}];",
        static_size=size,
        size_expr=str(size),
    )
    p.pack_lines += [
        "{",
        f"    size_t n = strlen(msg->{name});",
        f"    if (n != {size}) return OIGTL_ERR_FIELD_RANGE;",
        f"    memcpy(buf + off, msg->{name}, {size});",
        f"    off += {size};",
        "}",
    ]
    p.unpack_lines += [
        f"if (off + {size} > len) return OIGTL_ERR_SHORT_BUFFER;",
        f"memcpy(out->{name}, buf + off, {size});",
        f"out->{name}[{size}] = '\\0';",
        f"off += {size};",
    ]
    return p


def _plan_length_prefixed_string(name: str, prefix_type: str) -> FieldPlan:
    """uint16 length prefix + N bytes of payload.

    View pattern: pack reads ``msg->{name}`` + ``msg->{name}_len``;
    unpack sets those to a pointer into the wire buffer + the
    parsed length. The 16-bit prefix caps the payload at 65535
    bytes; we'll refuse anything longer at pack time.
    """
    if prefix_type != "uint16":
        raise NotImplementedError(
            f"length_prefixed_string with length_prefix_type="
            f"{prefix_type!r} — only uint16 is supported"
        )
    p = FieldPlan(
        name=name,
        declaration=(
            f"/* view: points into wire bytes — see README for lifetime */\n"
            f"    const char *{name};\n"
            f"    size_t      {name}_len;"
        ),
        static_size=None,
        size_expr=f"2 + msg->{name}_len",
    )
    p.pack_lines += [
        f"if (msg->{name}_len > 0xFFFFu) return OIGTL_ERR_FIELD_RANGE;",
        f"oigtl_write_be_u16(buf + off, (uint16_t)msg->{name}_len);",
        "off += 2;",
        f"if (msg->{name}_len > 0) {{",
        f"    memcpy(buf + off, msg->{name}, msg->{name}_len);",
        f"    off += msg->{name}_len;",
        "}",
    ]
    p.unpack_lines += [
        "if (off + 2 > len) return OIGTL_ERR_SHORT_BUFFER;",
        "{",
        "    size_t n = (size_t)oigtl_read_be_u16(buf + off);",
        "    off += 2;",
        "    if (off + n > len) return OIGTL_ERR_SHORT_BUFFER;",
        f"    out->{name} = (const char *)(buf + off);",
        f"    out->{name}_len = n;",
        "    off += n;",
        "}",
    ]
    return p


def _plan_trailing_string(name: str, null_terminated: bool) -> FieldPlan:
    """Free-text string that occupies the remainder of the body.

    View pattern: ``const char *name; size_t name_len;`` pointing
    into the wire buffer. With ``null_terminated``, the on-wire
    representation includes a trailing 0 byte that is NOT included
    in ``name_len``.
    """
    p = FieldPlan(
        name=name,
        declaration=(
            f"/* view: points into wire bytes — see README for lifetime */\n"
            f"    const char *{name};\n"
            f"    size_t      {name}_len;"
        ),
        static_size=None,
        size_expr=(
            f"msg->{name}_len + 1" if null_terminated
            else f"msg->{name}_len"
        ),
    )
    # Pack: copy the visible bytes, then (if null-terminated) a zero.
    p.pack_lines += [
        f"if (msg->{name}_len > 0) {{",
        f"    memcpy(buf + off, msg->{name}, msg->{name}_len);",
        f"    off += msg->{name}_len;",
        "}",
    ]
    if null_terminated:
        p.pack_lines += [
            "buf[off] = 0;",
            "off += 1;",
        ]
    # Unpack: take the remainder of the body as the view.
    if null_terminated:
        p.unpack_lines += [
            "{",
            "    size_t end = len;",
            "    if (end <= off) return OIGTL_ERR_MALFORMED;",
            "    if (buf[end - 1] != 0) return OIGTL_ERR_MALFORMED;",
            "    --end;",
            f"    out->{name} = (const char *)(buf + off);",
            f"    out->{name}_len = end - off;",
            "    off = len;",
            "}",
        ]
    else:
        p.unpack_lines += [
            f"out->{name} = (const char *)(buf + off);",
            f"out->{name}_len = len - off;",
            "off = len;",
        ]
    return p


# ---------------------------------------------------------------------------
# Dispatch: schema field dict → FieldPlan
# ---------------------------------------------------------------------------


def _plan_field(f: dict[str, Any]) -> FieldPlan:
    ftype = f["type"]
    name = f["name"]

    if ftype in _PRIMITIVE_C:
        return _plan_primitive(name, ftype)

    if ftype == "array":
        etype = f["element_type"]
        # element_type may be a string (primitive name) or a dict
        # (nested struct / fixed_string element). Round 1 supports
        # only primitive string names.
        if isinstance(etype, dict):
            raise NotImplementedError(
                f"array of struct/nested element_type not yet "
                f"supported (field {name!r})"
            )
        if etype not in _PRIMITIVE_C:
            raise NotImplementedError(
                f"array of non-primitive element_type={etype!r} "
                "not yet supported (field "
                f"{name!r})"
            )
        # `count` may be an int (compile-time fixed) or a string
        # (sibling-field reference — some schemas encode sibling-count
        # this way instead of via `count_from`).
        count = f.get("count")
        if isinstance(count, int):
            return _plan_array_fixed(name, etype, count)
        if isinstance(count, str):
            return _plan_array_sibling(name, etype, count)
        count_from = f.get("count_from")
        if count_from == "remaining":
            return _plan_array_remaining(name, etype)
        if isinstance(count_from, str):
            return _plan_array_sibling(name, etype, count_from)
        raise NotImplementedError(
            f"array field {name!r}: no supported count source"
        )

    if ftype == "fixed_string":
        size = int(f["size_bytes"])
        if f.get("null_padded", True):
            return _plan_fixed_string_null_padded(name, size)
        return _plan_fixed_string_raw(name, size)

    if ftype == "trailing_string":
        return _plan_trailing_string(
            name, bool(f.get("null_terminated", False)))

    if ftype == "length_prefixed_string":
        return _plan_length_prefixed_string(
            name, f.get("length_prefix_type", "uint16"))

    if ftype == "fixed_bytes":
        # Planned to support but not this round — schemas that use
        # fixed_bytes are uncommon and round 1 is tight.
        raise NotImplementedError(
            "fixed_bytes not yet supported in the C target")

    raise NotImplementedError(f"unsupported field type: {ftype!r}")


# ---------------------------------------------------------------------------
# Top-level entry
# ---------------------------------------------------------------------------


def plan_message(schema: dict[str, Any]) -> MessagePlan:
    type_id = schema["type_id"]
    fields_in = schema.get("fields", [])
    plans = [_plan_field(f) for f in fields_in]

    if all(p.static_size is not None for p in plans):
        body_size: Optional[int] = sum(p.static_size for p in plans)  # type: ignore[misc]
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
            " + ".join(f"({p.size_expr})" for p in plans) if plans else "0"
        )
        is_fixed_body = False

    body_size_set = schema.get("body_size_set")
    if body_size_set is not None:
        body_size_set = sorted(int(x) for x in body_size_set)

    return MessagePlan(
        type_id=type_id,
        struct_name=c_type_struct(type_id),
        fn_prefix=c_type_prefix(type_id),
        body_size_macro=c_body_size_macro(type_id),
        basename=c_header_basename(type_id),
        is_fixed_body=is_fixed_body,
        body_size=body_size,
        body_size_expr=body_size_expr,
        body_size_set=body_size_set,
        fields=plans,
        description=schema.get("description", ""),
    )
