"""Schema-type → TypeScript-type mapping + per-field pack/unpack planning.

The TS target emits inline imperative code (no runtime schema
walker), matching the shape of the C++ target rather than the
Python target. The planner here reduces each schema field to:

- The TS type annotation used in the class declaration
- A fragment of code that reads the field from a DataView at a
  running ``offset`` (``unpackFragment``)
- A fragment of code that appends the field's wire bytes to a
  ``parts: Uint8Array[]`` array for later concatenation
  (``packFragment``)

The emitter then splices the fragments into a single ``.ts`` file.

Only the schema shapes currently used by the 84 messages are
supported. Any unrecognized shape raises ``NotImplementedError``
with a descriptive message, matching the other codegen targets.
"""

from __future__ import annotations

from dataclasses import dataclass, field as dc_field
from typing import Any, Optional


# ---------------------------------------------------------------------------
# Primitive type mappings
# ---------------------------------------------------------------------------

# element_type → (TS scalar type, byte size, DataView getter, DataView setter,
#                 default literal, TypedArray class or None)
_PRIMITIVE: dict[str, tuple[str, int, str, str, str, Optional[str]]] = {
    "uint8":   ("number", 1, "readU8",  "writeU8",  "0",  None),
    "int8":    ("number", 1, "readI8",  "writeI8",  "0",  None),
    "uint16":  ("number", 2, "readU16", "writeU16", "0",  "Uint16Array"),
    "int16":   ("number", 2, "readI16", "writeI16", "0",  "Int16Array"),
    "uint32":  ("number", 4, "readU32", "writeU32", "0",  "Uint32Array"),
    "int32":   ("number", 4, "readI32", "writeI32", "0",  "Int32Array"),
    "uint64":  ("bigint", 8, "readU64", "writeU64", "0n", "BigUint64Array"),
    "int64":   ("bigint", 8, "readI64", "writeI64", "0n", "BigInt64Array"),
    "float32": ("number", 4, "readF32", "writeF32", "0",  "Float32Array"),
    "float64": ("number", 8, "readF64", "writeF64", "0",  "Float64Array"),
}

_PRIMITIVE_TYPEDARRAY_READ: dict[str, str] = {
    "uint16":  "readU16ArrayBE",
    "int16":   "readI16ArrayBE",
    "uint32":  "readU32ArrayBE",
    "int32":   "readI32ArrayBE",
    "uint64":  "readU64ArrayBE",
    "int64":   "readI64ArrayBE",
    "float32": "readF32ArrayBE",
    "float64": "readF64ArrayBE",
}

_PRIMITIVE_TYPEDARRAY_WRITE: dict[str, str] = {
    "uint16":  "writeU16ArrayBE",
    "int16":   "writeI16ArrayBE",
    "uint32":  "writeU32ArrayBE",
    "int32":   "writeI32ArrayBE",
    "uint64":  "writeU64ArrayBE",
    "int64":   "writeI64ArrayBE",
    "float32": "writeF32ArrayBE",
    "float64": "writeF64ArrayBE",
}


# Names that collide with built-ins (global `String`, DOM `Image`,
# `Header` from the runtime module, etc.). For these we append
# `Message` to keep the generated class from shadowing the platform
# type. Everything else stays as the natural CamelCase form.
_TS_RESERVED_NAMES: frozenset[str] = frozenset({
    "String",
    "Image",
    "Header",
    "ExtHeader",
    "Video",
    "Number",
    "Date",
    "Array",
    "Object",
    "Map",
    "Set",
    "Request",
    "Response",
})


def _ts_class_name(type_id: str) -> str:
    """``"TRANSFORM"`` → ``"Transform"``; ``"STT_TRANSFORM"`` → ``"SttTransform"``.

    Appends ``Message`` for names that would collide with built-ins
    or runtime exports (see `_TS_RESERVED_NAMES`).
    """
    base = "".join(part.capitalize() for part in type_id.split("_"))
    if base in _TS_RESERVED_NAMES:
        return base + "Message"
    return base


def _ts_module_name(type_id: str) -> str:
    """Module file stem — lowercased, underscore form preserved."""
    return type_id.lower()


def _singularize(word: str) -> str:
    if word.endswith("ies") and len(word) > 3:
        return word[:-3] + "y"
    if word.endswith("s") and not word.endswith("ss") and len(word) > 1:
        return word[:-1]
    return word


def _nested_interface_name(field_name: str) -> str:
    parts = field_name.split("_")
    parts[-1] = _singularize(parts[-1])
    return "".join(p.capitalize() for p in parts) + "Entry"


# ---------------------------------------------------------------------------
# Dataclasses — what the template consumes
# ---------------------------------------------------------------------------


@dataclass
class NestedSubField:
    name: str
    ts_type: str
    default_literal: str
    unpack_code: str        # "fld = readF32(view, offset); offset += 4;"
    pack_code: str          # "writeF32(partView, partOff, x.fld); partOff += 4;"
    fixed_size: int         # byte size for this sub-field (struct elements
                            # must be statically sized — matches other targets).


@dataclass
class NestedInterface:
    name: str               # e.g. "PointsEntry"
    fields: list[NestedSubField]
    total_size: int         # sum of sub-field sizes

    @property
    def size_expr(self) -> str:
        return str(self.total_size)


@dataclass
class TopField:
    name: str
    ts_type: str
    default_literal: str
    unpack_code: str
    pack_code: str


@dataclass
class MessageTsPlan:
    type_id: str
    class_name: str
    module_name: str
    body_size: Optional[int]
    is_fixed_body: bool
    fields: list[TopField]
    nested_interfaces: list[NestedInterface] = dc_field(default_factory=list)
    body_size_set: Optional[list[int]] = None


# ---------------------------------------------------------------------------
# Field planners
# ---------------------------------------------------------------------------


def _unsupported(what: str) -> NotImplementedError:
    return NotImplementedError(f"TS codegen: {what} not supported")


def _string_default(_field: dict[str, Any]) -> str:
    return '""'


def _plan_nested_sub(sub: dict[str, Any], local_offset: int) -> NestedSubField:
    t = sub["type"]
    name = sub["name"]

    if t in _PRIMITIVE:
        ts_t, size, getter, setter, default, _ = _PRIMITIVE[t]
        unpack = f"    const sub_{name} = {getter}(view, offset + {local_offset});"
        pack = f"    {setter}(partView, partOff + {local_offset}, x.{name});"
        return NestedSubField(
            name=name,
            ts_type=ts_t,
            default_literal=default,
            unpack_code=unpack,
            pack_code=pack,
            fixed_size=size,
        )

    if t == "fixed_string":
        size = sub["size_bytes"]
        null_padded = sub.get("null_padded", True)
        unpack_helper = "_readAscii" if null_padded else "_readAsciiRaw"
        unpack = (
            f"    const sub_{name} = {unpack_helper}(bytes, offset + "
            f"{local_offset}, {size});"
        )
        pack = (
            f"    _writeAscii(partBytes, partOff + {local_offset}, "
            f"{size}, x.{name});"
        )
        return NestedSubField(
            name=name,
            ts_type="string",
            default_literal='""',
            unpack_code=unpack,
            pack_code=pack,
            fixed_size=size,
        )

    if t == "array":
        et = sub.get("element_type")
        count = sub.get("count")
        if isinstance(et, str) and et in _PRIMITIVE and isinstance(count, int):
            if et == "uint8":
                size = count
                # Copy to own buffer so the nested object can outlive
                # the input buffer safely.
                unpack = (
                    f"    const sub_{name} = new Uint8Array(bytes.subarray("
                    f"offset + {local_offset}, offset + {local_offset} + "
                    f"{size}));"
                )
                pack = (
                    f"    partBytes.set(x.{name}, partOff + {local_offset});"
                )
                return NestedSubField(
                    name=name,
                    ts_type="Uint8Array",
                    default_literal=f"new Uint8Array({count})",
                    unpack_code=unpack,
                    pack_code=pack,
                    fixed_size=size,
                )
            ts_t, elem_size, getter, setter, _, _ = _PRIMITIVE[et]
            size = elem_size * count
            unpack_lines = [
                f"    const sub_{name}: {ts_t}[] = new Array({count});",
                f"    for (let _i = 0; _i < {count}; _i++) sub_{name}[_i] = "
                f"{getter}(view, offset + {local_offset} + _i * {elem_size});",
            ]
            pack_lines = [
                f"    for (let _i = 0; _i < {count}; _i++) "
                f"{setter}(partView, partOff + {local_offset} + _i * "
                f"{elem_size}, x.{name}[_i] as {ts_t});",
            ]
            return NestedSubField(
                name=name,
                ts_type=f"{ts_t}[]",
                default_literal=f"new Array<{ts_t}>({count}).fill("
                                f"{_PRIMITIVE[et][4]})",
                unpack_code="\n".join(unpack_lines),
                pack_code="\n".join(pack_lines),
                fixed_size=size,
            )

    raise _unsupported(f"struct sub-field {name!r} type {t!r}")


def _plan_top_field(
    f: dict[str, Any],
    nested_for_field: dict[str, NestedInterface],
) -> TopField:
    t = f["type"]
    name = f["name"]

    if t in _PRIMITIVE:
        ts_t, size, getter, setter, default, _ = _PRIMITIVE[t]
        unpack = (
            f"    const {name} = {getter}(view, offset); offset += {size};"
        )
        # pack: append a fresh slice of `size` bytes
        pack_lines = [
            f"    {{",
            f"      const part = new Uint8Array({size});",
            f"      {setter}(viewOf(part), 0, this.{name});",
            f"      parts.push(part);",
            f"    }}",
        ]
        return TopField(
            name=name,
            ts_type=ts_t,
            default_literal=default,
            unpack_code=unpack,
            pack_code="\n".join(pack_lines),
        )

    if t == "fixed_string":
        size = f["size_bytes"]
        null_padded = f.get("null_padded", True)
        unpack_helper = "_readAscii" if null_padded else "_readAsciiRaw"
        unpack = (
            f"    const {name} = {unpack_helper}(bytes, offset, {size}); "
            f"offset += {size};"
        )
        pack_lines = [
            f"    {{",
            f"      const part = new Uint8Array({size});",
            f"      _writeAscii(part, 0, {size}, this.{name});",
            f"      parts.push(part);",
            f"    }}",
        ]
        return TopField(
            name=name,
            ts_type="string",
            default_literal='""',
            unpack_code=unpack,
            pack_code="\n".join(pack_lines),
        )

    if t == "fixed_bytes":
        size = f["size_bytes"]
        unpack = (
            f"    const {name} = new Uint8Array(bytes.subarray("
            f"offset, offset + {size})); offset += {size};"
        )
        pack_lines = [
            f"    {{",
            f"      const part = new Uint8Array({size});",
            f"      part.set(this.{name}.subarray(0, {size}));",
            f"      parts.push(part);",
            f"    }}",
        ]
        return TopField(
            name=name,
            ts_type="Uint8Array",
            default_literal=f"new Uint8Array({size})",
            unpack_code=unpack,
            pack_code="\n".join(pack_lines),
        )

    if t == "length_prefixed_string":
        prefix = f["length_prefix_type"]
        if prefix not in _PRIMITIVE:
            raise _unsupported(f"length_prefix_type {prefix!r}")
        _, prefix_size, getter, setter, _, _ = _PRIMITIVE[prefix]
        enc = f.get("encoding", "ascii")
        binary = enc == "binary"
        if binary:
            ts_type = "Uint8Array"
            default = "new Uint8Array(0)"
            unpack_body = (
                f"new Uint8Array(bytes.subarray(offset, offset + _len))"
            )
            pack_payload = f"this.{name}"
            pack_len_expr = f"this.{name}.length"
        elif enc == "ascii":
            # Strict ASCII — matches Python's bytes.decode("ascii").
            # Neither browser nor Node's TextDecoder rejects non-ASCII
            # on label "ascii" (aliased to windows-1252 per the
            # Encoding spec), so we can't rely on it here.
            ts_type = "string"
            default = '""'
            unpack_body = f"_readAsciiRaw(bytes, offset, _len)"
            pack_payload = f"_encodeAscii(this.{name})"
            pack_len_expr = f"_payload_{name}.length"
        else:
            ts_type = "string"
            default = '""'
            unpack_body = (
                f"new TextDecoder(\"{enc}\", {{ fatal: true }}).decode("
                f"bytes.subarray(offset, offset + _len))"
            )
            pack_payload = f"new TextEncoder().encode(this.{name})"
            pack_len_expr = f"_payload_{name}.length"
        unpack_lines = [
            f"    const _len = Number({getter}(view, offset)); "
            f"offset += {prefix_size};",
            f"    const {name} = {unpack_body}; offset += _len;",
        ]
        if binary:
            pack_lines = [
                f"    {{",
                f"      const _len = {pack_len_expr};",
                f"      const part = new Uint8Array({prefix_size} + _len);",
                f"      {setter}(viewOf(part), 0, {('BigInt(_len)' if prefix in ('uint64','int64') else '_len')});",
                f"      part.set({pack_payload}, {prefix_size});",
                f"      parts.push(part);",
                f"    }}",
            ]
        else:
            pack_lines = [
                f"    {{",
                f"      const _payload_{name} = {pack_payload};",
                f"      const _len = {pack_len_expr};",
                f"      const part = new Uint8Array({prefix_size} + _len);",
                f"      {setter}(viewOf(part), 0, {('BigInt(_len)' if prefix in ('uint64','int64') else '_len')});",
                f"      part.set(_payload_{name}, {prefix_size});",
                f"      parts.push(part);",
                f"    }}",
            ]
        return TopField(
            name=name,
            ts_type=ts_type,
            default_literal=default,
            unpack_code="\n".join(unpack_lines),
            pack_code="\n".join(pack_lines),
        )

    if t == "trailing_string":
        enc = f.get("encoding", "ascii")
        null_terminated = f.get("null_terminated", False)
        binary = enc == "binary"
        if binary:
            ts_type = "Uint8Array"
            default = "new Uint8Array(0)"
        else:
            ts_type = "string"
            default = '""'
        if binary:
            unpack_body = (
                "new Uint8Array(bytes.subarray(offset, bytes.length"
                f"{' - 1' if null_terminated else ''}))"
            )
            pack_payload = f"this.{name}"
        elif enc == "ascii":
            # Strict ASCII — see length_prefixed_string for rationale.
            end_expr = (
                "bytes.length - 1" if null_terminated else "bytes.length"
            )
            unpack_body = f"_readAsciiRaw(bytes, offset, {end_expr} - offset)"
            pack_payload = f"_encodeAscii(this.{name})"
        else:
            unpack_body = (
                f"new TextDecoder(\"{enc}\", {{ fatal: true }}).decode("
                f"bytes.subarray(offset, bytes.length"
                f"{' - 1' if null_terminated else ''}))"
            )
            pack_payload = f"new TextEncoder().encode(this.{name})"
        unpack_lines = [
            f"    const {name} = {unpack_body}; offset = bytes.length;",
        ]
        if null_terminated:
            pack_lines = [
                f"    {{",
                f"      const _payload = {pack_payload};",
                f"      const part = new Uint8Array(_payload.length + 1);",
                f"      part.set(_payload, 0);",
                f"      parts.push(part);",
                f"    }}",
            ]
        else:
            pack_lines = [
                f"    {{",
                f"      const _payload = {pack_payload};",
                f"      parts.push({'_payload' if binary else 'new Uint8Array(_payload)'});",
                f"    }}",
            ]
        return TopField(
            name=name,
            ts_type=ts_type,
            default_literal=default,
            unpack_code="\n".join(unpack_lines),
            pack_code="\n".join(pack_lines),
        )

    if t in ("array", "struct_array"):
        return _plan_array_field(f, nested_for_field)

    raise _unsupported(f"field type {t!r}")


def _plan_array_field(
    f: dict[str, Any],
    nested_for_field: dict[str, NestedInterface],
) -> TopField:
    name = f["name"]
    et = f["element_type"]
    count = f.get("count")
    count_from = f.get("count_from")

    # --- Fixed or dynamic count resolution ---
    if isinstance(count, int):
        count_expr = str(count)
        is_fixed_count = True
    elif isinstance(count, str):
        # Sibling field name — must have been unpacked earlier.
        count_expr = f"Number({count})"
        is_fixed_count = False
    elif count_from == "remaining":
        count_expr = None  # special-case below
        is_fixed_count = False
    else:
        raise _unsupported(
            f"array {name!r}: neither count (int|sibling) nor "
            f"count_from='remaining'"
        )

    # --- Primitive element ---
    if isinstance(et, str) and et in _PRIMITIVE:
        elem_size = _PRIMITIVE[et][1]
        ts_scalar = _PRIMITIVE[et][0]

        if et == "uint8":
            # Always Uint8Array; no endian concern.
            if is_fixed_count:
                unpack = (
                    f"    const {name} = new Uint8Array(bytes.subarray("
                    f"offset, offset + {count_expr})); offset += {count_expr};"
                )
                default = f"new Uint8Array({count_expr})"
            elif count_expr is not None:
                unpack = (
                    f"    const _n_{name} = {count_expr};\n"
                    f"    const {name} = new Uint8Array(bytes.subarray("
                    f"offset, offset + _n_{name})); offset += _n_{name};"
                )
                default = "new Uint8Array(0)"
            else:
                # remaining
                unpack = (
                    f"    const {name} = new Uint8Array(bytes.subarray("
                    f"offset, bytes.length)); offset = bytes.length;"
                )
                default = "new Uint8Array(0)"
            pack = (
                f"    parts.push(new Uint8Array(this.{name}));"
            )
            return TopField(
                name=name,
                ts_type="Uint8Array",
                default_literal=default,
                unpack_code=unpack,
                pack_code=pack,
            )

        # Non-uint8 primitives.
        if is_fixed_count:
            # Fixed-count stays as plain array (small N, ergonomic).
            getter = _PRIMITIVE[et][2]
            setter = _PRIMITIVE[et][3]
            n = count_expr
            unpack = (
                f"    const {name}: {ts_scalar}[] = new Array({n});\n"
                f"    for (let _i = 0; _i < {n}; _i++) {{ {name}[_i] = "
                f"{getter}(view, offset); offset += {elem_size}; }}"
            )
            pack_lines = [
                f"    {{",
                f"      const part = new Uint8Array({n} * {elem_size});",
                f"      const pv = viewOf(part);",
                f"      for (let _i = 0; _i < {n}; _i++) "
                f"{setter}(pv, _i * {elem_size}, this.{name}[_i] as {ts_scalar});",
                f"      parts.push(part);",
                f"    }}",
            ]
            default_fill = _PRIMITIVE[et][4]
            return TopField(
                name=name,
                ts_type=f"{ts_scalar}[]",
                default_literal=f"new Array<{ts_scalar}>({n}).fill({default_fill})",
                unpack_code=unpack,
                pack_code="\n".join(pack_lines),
            )

        # Variable-count non-uint8 primitive → TypedArray.
        typed_array = _PRIMITIVE[et][5]
        assert typed_array is not None
        read_fn = _PRIMITIVE_TYPEDARRAY_READ[et]
        write_fn = _PRIMITIVE_TYPEDARRAY_WRITE[et]

        if count_expr is not None:
            unpack = (
                f"    const _n_{name} = {count_expr};\n"
                f"    const {name} = {read_fn}(view, offset, _n_{name}); "
                f"offset += _n_{name} * {elem_size};"
            )
        else:
            # remaining
            unpack = (
                f"    const _n_{name} = (bytes.length - offset) / {elem_size};\n"
                f"    if (!Number.isInteger(_n_{name})) throw new BodyDecodeError("
                f"`{name}: remaining {{bytes.length - offset}} bytes not "
                f"divisible by element size {elem_size}`);\n"
                f"    const {name} = {read_fn}(view, offset, _n_{name}); "
                f"offset += _n_{name} * {elem_size};"
            )
        pack_lines = [
            f"    {{",
            f"      const part = new Uint8Array(this.{name}.length * {elem_size});",
            f"      {write_fn}(viewOf(part), 0, this.{name});",
            f"      parts.push(part);",
            f"    }}",
        ]
        return TopField(
            name=name,
            ts_type=typed_array,
            default_literal=f"new {typed_array}(0)",
            unpack_code=unpack,
            pack_code="\n".join(pack_lines),
        )

    # --- fixed_string element ---
    if isinstance(et, dict) and et.get("type") == "fixed_string":
        size = et["size_bytes"]
        null_padded = et.get("null_padded", True)
        reader = "_readAscii" if null_padded else "_readAsciiRaw"
        if count_expr is None:
            # remaining → count = (len-off)/size
            unpack = (
                f"    const _n_{name} = (bytes.length - offset) / {size};\n"
                f"    const {name}: string[] = new Array(_n_{name});\n"
                f"    for (let _i = 0; _i < _n_{name}; _i++) {{ "
                f"{name}[_i] = {reader}(bytes, offset, {size}); "
                f"offset += {size}; }}"
            )
        else:
            unpack = (
                f"    const _n_{name} = {count_expr};\n"
                f"    const {name}: string[] = new Array(_n_{name});\n"
                f"    for (let _i = 0; _i < _n_{name}; _i++) {{ "
                f"{name}[_i] = {reader}(bytes, offset, {size}); "
                f"offset += {size}; }}"
            )
        pack_lines = [
            f"    {{",
            f"      const part = new Uint8Array(this.{name}.length * {size});",
            f"      for (let _i = 0; _i < this.{name}.length; _i++) "
            f"_writeAscii(part, _i * {size}, {size}, this.{name}[_i] as string);",
            f"      parts.push(part);",
            f"    }}",
        ]
        return TopField(
            name=name,
            ts_type="string[]",
            default_literal="[]",
            unpack_code=unpack,
            pack_code="\n".join(pack_lines),
        )

    # --- struct element ---
    if isinstance(et, dict) and et.get("type") == "struct":
        nested = nested_for_field[name]
        elem_size = nested.total_size
        if count_expr is None:
            n_setup = (
                f"    const _n_{name} = (bytes.length - offset) / "
                f"{elem_size};\n"
                f"    if (!Number.isInteger(_n_{name})) throw new BodyDecodeError("
                f"`{name}: remaining bytes not divisible by element size "
                f"{elem_size}`);"
            )
        else:
            n_setup = f"    const _n_{name} = {count_expr};"
        # Build unpack: loop, construct each NestedInterface entry.
        sub_unpacks = "\n".join(s.unpack_code for s in nested.fields)
        ctor_fields = ", ".join(
            f"{s.name}: sub_{s.name}" for s in nested.fields
        )
        unpack = "\n".join([
            n_setup,
            f"    const {name}: {nested.name}[] = new Array(_n_{name});",
            f"    for (let _i = 0; _i < _n_{name}; _i++) {{",
            sub_unpacks,
            f"      {name}[_i] = {{ {ctor_fields} }};",
            f"      offset += {elem_size};",
            f"    }}",
        ])
        sub_packs = "\n".join(s.pack_code for s in nested.fields)
        pack_lines = [
            f"    {{",
            f"      const part = new Uint8Array(this.{name}.length * {elem_size});",
            f"      const pv = viewOf(part);",
            f"      let partOff = 0;",
            f"      const partBytes = part;",
            f"      const partView = pv;",
            f"      for (const x of this.{name}) {{",
            sub_packs,
            f"        partOff += {elem_size};",
            f"      }}",
            f"      parts.push(part);",
            f"    }}",
        ]
        return TopField(
            name=name,
            ts_type=f"{nested.name}[]",
            default_literal="[]",
            unpack_code=unpack,
            pack_code="\n".join(pack_lines),
        )

    raise _unsupported(f"array element_type {et!r} for field {name!r}")


# ---------------------------------------------------------------------------
# Whole-message planner
# ---------------------------------------------------------------------------


def plan_message(schema: dict[str, Any]) -> MessageTsPlan:
    type_id = schema["type_id"]
    class_name = _ts_class_name(type_id)
    fields_in = schema.get("fields", [])

    # Discover nested struct interfaces up front (they may be
    # referenced by the top-level planner).
    nested_for_field: dict[str, NestedInterface] = {}
    for f in fields_in:
        et = f.get("element_type")
        if isinstance(et, dict) and et.get("type") == "struct":
            subs: list[NestedSubField] = []
            running = 0
            for sf in et["fields"]:
                sub = _plan_nested_sub(sf, running)
                subs.append(sub)
                running += sub.fixed_size
            iface = NestedInterface(
                name=_nested_interface_name(f["name"]),
                fields=subs,
                total_size=running,
            )
            nested_for_field[f["name"]] = iface

    top_fields = [_plan_top_field(f, nested_for_field) for f in fields_in]

    body_size = schema.get("body_size")
    is_fixed_body = isinstance(body_size, int)

    body_size_set = schema.get("body_size_set")
    if body_size_set is not None:
        body_size_set = sorted(int(x) for x in body_size_set)

    return MessageTsPlan(
        type_id=type_id,
        class_name=class_name,
        module_name=_ts_module_name(type_id),
        body_size=body_size if is_fixed_body else None,
        is_fixed_body=is_fixed_body,
        fields=top_fields,
        nested_interfaces=list(nested_for_field.values()),
        body_size_set=body_size_set,
    )
