# core-py numpy-native bulk arrays — plan

Status: **complete (Phases 1-4)**. Phase 5 (POLYDATA struct-array
flattening) remains out of scope and deferred.

Summary of what shipped:

- Codec returns raw big-endian wire bytes for variable-count
  primitive fields (all types, not just uint8).
- `oigtl.runtime.arrays` coerces those bytes to `np.ndarray` (with
  the `[numpy]` extra) or `array.array` (stdlib fallback), keyed on
  wire element type.
- Python codegen emits `@field_validator` + `pack()` preprocessing
  for every variable-count non-uint8 primitive field.
- `oigtl.semantic.pixel_array(img)` / `data_array(nd)` give users
  a one-liner for the IMAGE/NDARRAY reshape-and-dtype flow.
- `[numpy]` packaging extra added; benchmarks in
  `core-py/benches/bench_numpy.py`.
- Total core-py tests: 126 → 149 (23 new in
  `test_variable_arrays.py` + `test_semantic.py`).

Historical plan retained below.

---

## Goal

Extend the typed Python library so that variable-count primitive
arrays use a compact typed representation — `np.ndarray` when the
user has opted into the `[numpy]` extra, `array.array` from the
stdlib otherwise — instead of boxed `list[int]` / `list[float]`.
Add application-layer convenience helpers for the fields where
the wire element type and the semantic element type differ
(`IMAGE.pixels`, `NDARRAY.data`, `VIDEO.frame`).

Acceptance criterion: **a realistic imaging server processing
1920×1080 grayscale frames at 30 fps runs comfortably in the
Python typed library** (today's `list[int]` representation makes
this impossible — ~22 ms per FHD unpack). Without the `[numpy]`
extra, the same code must still work correctly, just slower.

## Motivation

Current state of the typed Python library after the uint8-as-bytes
optimization:

| Wire shape | Current Python type | Perf on 1M elements |
|---|---|---|
| `uint8[N]` variable | `bytes` | ~500 µs (optimal) |
| `int16[N]` / `float32[N]` variable | `list[int]` / `list[float]` | ~60 ms + 28 MB heap |
| `uint8[N]` fixed | `bytes` | irrelevant (small) |
| `float32[N]` fixed | `list[float]` | irrelevant (small) |
| Struct array | `list[NestedModel]` | N/A for this plan |

The non-uint8 variable-count case is the gap. It hits SENSOR.data
(float64) for users streaming sensor arrays, and conceptually
blocks any future extension where a variable-length float/int
array becomes a primary use case.

A second, related gap: `IMAGE.pixels` is `bytes` at the wire level
but users want a numpy array with the correct dtype (per
`scalar_type`) and shape (per `size`). Currently they must write
`np.frombuffer(img.pixels, dtype=_map(img.scalar_type)).reshape(img.size[::-1])`
by hand every time. This is pyigtl's `ImageMessage.image` property.

## Representation rules

Four rules, unambiguous:

| Wire shape | With `[numpy]` | Without `[numpy]` |
|---|---|---|
| `uint8` (any count) | `bytes` | `bytes` |
| Fixed-count non-uint8 primitive | `list[T]` | `list[T]` |
| Variable-count non-uint8 primitive | `np.ndarray` (dtype=`>f4`/`>i2`/…) | `array.array` (byteswapped) |
| Struct-element array | `list[NestedModel]` | `list[NestedModel]` |

Key properties:

- **`bytes` stays `bytes` for uint8** regardless of extra. It's
  Python's native opaque-binary-data type, works with hashlib,
  pickle, networking libraries, and already-written user code.
  Users wanting numpy view of pixels use `np.frombuffer(...)` —
  one line.
- **Fixed-count arrays stay as `list[T]`** regardless of extra.
  Counts in our 84 schemas are always ≤12 (TRANSFORM.matrix).
  The crossover where numpy pays off is around N=30-50; below
  that, list construction is cheaper. Simpler to apply one rule
  ("variable = numpy/array.array, fixed = list") than to find a
  per-type threshold.
- **Variable-count non-uint8 is where the performance win lives.**
  Same user-visible API either way (`msg.field[i]`, `len()`,
  iteration, slicing all work); only the underlying container
  differs.

### array.array type codes

Avoid `'l'` / `'L'` (C `long`, platform-dependent size). Use
`'q'` / `'Q'` for 64-bit ints (Python 3.3+).

```python
_ARRAY_CODE = {
    "int8":    "b", "uint8":   "B",
    "int16":   "h", "uint16":  "H",
    "int32":   "i", "uint32":  "I",
    "int64":   "q", "uint64":  "Q",
    "float32": "f", "float64": "d",
}
```

Assert at module import that itemsize matches expected wire sizes;
fail loudly on exotic platforms.

### numpy big-endian dtype mapping

```python
_NUMPY_BE_DTYPE = {
    "int8":    np.int8,    "uint8":   np.uint8,     # 1-byte, endian irrelevant
    "int16":   ">i2",      "uint16":  ">u2",
    "int32":   ">i4",      "uint32":  ">u4",
    "int64":   ">i8",      "uint64":  ">u8",
    "float32": ">f4",      "float64": ">f8",
}
```

`np.frombuffer(bytes, dtype='>f4')` is zero-copy. Resulting array
is non-native-endian; numpy handles arithmetic transparently. For
users who want native-endian (faster downstream math), `.astype(np.float32)`
copies once.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Pydantic typed class                                       │
│    field type: Any (with arbitrary_types_allowed=True)       │
│    @field_validator coerces input → canonical runtime type  │
│      canonical type = ndarray | array.array | bytes | list │
│      by (numpy availability, wire element_type, fixed/var) │
├─────────────────────────────────────────────────────────────┤
│  oigtl.runtime.arrays (new module, ~80 LoC)                 │
│    - _HAS_NUMPY flag                                        │
│    - _ARRAY_CODE / _NUMPY_BE_DTYPE maps                     │
│    - coerce_variable_array(value, element_type) -> typed    │
│    - pack_variable_array(value, element_type) -> bytes      │
├─────────────────────────────────────────────────────────────┤
│  oigtl_corpus_tools.codec.fields (modified)                 │
│    - variable-count primitive path returns bytes            │
│    - fixed-count stays list (unchanged)                     │
│    - pack accepts bytes/ndarray/array.array/list            │
└─────────────────────────────────────────────────────────────┘
```

## Phased work

Each phase is self-contained and individually shippable.

### Phase 1 — Codec: bytes for all variable-count primitives (0.5 days)

In `corpus-tools/src/oigtl_corpus_tools/codec/fields.py`, the
unpack path currently special-cases uint8. Extend to all primitives
with variable count; keep fixed-count unchanged.

**Change shape:**

```python
# Variable-count path in _unpack_field (count is sibling or remaining):
if element_type == "uint8":
    return bytes(data[offset:offset + count]), offset + count
if isinstance(element_type, str) and element_type in FORMAT:
    elem_size = SIZE[element_type]
    return bytes(data[offset:offset + elem_size * count]), offset + elem_size * count

# Fixed-count primitive path (count is int):
# unchanged — still returns list
```

**Pack side:** `pack_fields` already handles bytes/ndarray/array.array
for uint8 (all three support `bytes(value)`). For non-uint8 bytes
input, add a fast pass-through:

```python
if isinstance(value, (bytes, bytearray)):
    expected = SIZE[element_type] * (len(value) // SIZE[element_type])
    assert len(value) == expected, "byte length must match element count"
    return bytes(value)    # already wire-format big-endian
```

For ndarray, use numpy's built-in endian coercion:
```python
if isinstance(value, np.ndarray):
    return value.astype(f">{dtype_code}").tobytes()
```

For array.array, byteswap and serialize:
```python
if isinstance(value, array.array):
    if element_type not in ("int8", "uint8") and sys.byteorder == "little":
        swapped = array.array(value.typecode, value)
        swapped.byteswap()
        return swapped.tobytes()
    return value.tobytes()
```

**Test updates** in `corpus-tools/tests/test_codec.py` and
`tests/test_oracle.py`: assertions that compared unpacked values
to `list[float]` / `list[int]` need to compare against `bytes` or
convert bytes to list before comparison. Round-trip tests are
unaffected (they compare bytes, not intermediate structures).

**Exit criterion:** all 110 corpus-tools tests pass after updates.
The unpacked-value shape changes but the round-trip guarantee is
preserved.

### Phase 2 — Runtime helpers + Pydantic validators (1 day)

New file `core-py/src/oigtl/runtime/arrays.py`:

```python
from __future__ import annotations
import array
import sys
from typing import Any

try:
    import numpy as np
    _HAS_NUMPY = True
except ImportError:
    _HAS_NUMPY = False

_ARRAY_CODE = {...}                 # table above
_NUMPY_BE_DTYPE = {...} if _HAS_NUMPY else {}

# Portability assertions
assert array.array("i").itemsize == 4
assert array.array("q").itemsize == 8

def coerce_variable_array(value: Any, element_type: str) -> Any:
    """Coerce bytes/list/array.array/ndarray → preferred runtime type.

    Variable-count primitive fields from the codec arrive as bytes.
    This function promotes them to ndarray (numpy available) or
    array.array (fallback). Users who pass list/ndarray/array.array
    on construction get the same normalization.
    """
    if element_type == "uint8":
        # uint8 stays as bytes regardless of extra — preserves
        # compatibility with hashlib, pickle, networking libs.
        if isinstance(value, bytes):
            return value
        return bytes(value)

    if _HAS_NUMPY:
        if isinstance(value, np.ndarray):
            return value
        if isinstance(value, bytes):
            return np.frombuffer(value, dtype=_NUMPY_BE_DTYPE[element_type])
        if isinstance(value, (list, array.array)):
            return np.asarray(value, dtype=_NUMPY_BE_DTYPE[element_type])
        raise TypeError(f"cannot coerce {type(value)} to ndarray")

    if isinstance(value, array.array):
        if value.typecode != _ARRAY_CODE[element_type]:
            # wrong typecode — reinterpret via bytes
            return coerce_variable_array(value.tobytes(), element_type)
        return value
    if isinstance(value, bytes):
        arr = array.array(_ARRAY_CODE[element_type])
        arr.frombytes(value)
        if element_type not in ("int8", "uint8") and sys.byteorder == "little":
            arr.byteswap()
        return arr
    if isinstance(value, list):
        return array.array(_ARRAY_CODE[element_type], value)
    raise TypeError(f"cannot coerce {type(value)} to array.array")
```

Update `corpus-tools/.../codegen/python_types.py` to emit validators
for variable-count non-uint8 primitive fields:

```python
# In _plan_top_field, when et is non-uint8 primitive and count is
# sibling-string or None+count_from="remaining":
py_t = "Any"
init = "Field(default_factory=lambda: _make_empty_array(<element_type>))"
validator_lines = [
    f"@field_validator({name!r}, mode='before')",
    f"@classmethod",
    f"def _validate_{name}(cls, v): return coerce_variable_array(v, {element_type!r})",
]
```

The message template adds necessary imports and appends the validator
methods to the class.

Fixed-count non-uint8 primitives keep their current `list[T]` field
type — no validator change.

**Exit criterion:** `core-py` tests pass. `msg.field` on a
variable-count float/int field returns `np.ndarray` when numpy is
available, `array.array` otherwise. Indexing (`msg.field[i]`),
iteration, and `len()` all produce semantically-correct values in
both modes. Round-trip byte-for-byte preserved for all 23
supported fixtures.

### Phase 3 — Packaging: `[numpy]` extra + bench (0.5 days)

In `core-py/pyproject.toml`:

```toml
[project.optional-dependencies]
numpy = ["numpy>=1.24"]
```

Add `core-py/benches/bench_numpy.py` (new file or extend
`bench_typed.py`) that measures unpack/pack for SENSOR with 100/1K/10K
float64 elements, with and without numpy installed. Document numbers
in `core-py/README.md`.

Update `core-py/README.md` performance section to include:
- The representation rules table
- Install instructions for `pip install oigtl` vs `pip install 'oigtl[numpy]'`
- Bench numbers showing the three-tier (list / array.array / ndarray) perf

**Exit criterion:** bench confirms >10× speedup for SENSOR.data at
1K+ elements with `[numpy]`, >5× without. No regression on control
messages (TRANSFORM at 60Hz).

### Phase 4 — Semantic-type helpers for IMAGE / NDARRAY / VIDEO (0.5 days)

Convenience methods on the typed classes where wire element type
(`uint8`) differs from semantic element type (`int16`, `float32`,
etc.). These live on individual message classes via codegen
extensions, not on the generic primitive-array path.

```python
# Generated on Image (via a schema hint or hardcoded special-case):
_SCALAR_DTYPE = {
    2: np.int8,  3: np.uint8,  4: np.int16,  5: np.uint16,
    6: np.int32, 7: np.uint32, 10: np.float32, 11: np.float64,
}

def pixel_array(self) -> np.ndarray:
    """Return pixels reshaped + dtyped per scalar_type and size.

    Requires numpy. Raises ImportError otherwise.
    """
    import numpy as np
    dtype = _SCALAR_DTYPE[self.scalar_type]
    arr = np.frombuffer(self.pixels, dtype=dtype)
    # size is (i, j, k) on the wire; numpy convention is (k, j, i)
    return arr.reshape(tuple(reversed(self.size)))
```

Same pattern for `NDArray.data_array()` and (when we get there)
`Video.frame_array()`. Honor the IMAGE `endian` field with a
warning on `endian=1` (see interop notes in response thread —
pyigtl ignores this field, most real deployments are endian=2,
strict spec interpretation would flip bytes).

**Decision:** hardcode the special-case in codegen for these three
types (small list, stable over time) vs. add a schema-level hint
(`semantic_reshape: {dtype_from: scalar_type, shape_from: size}`).
Recommendation: hardcode for now, promote to schema hint if a
fourth message type ends up needing it.

**Exit criterion:** `IMAGE.pixel_array()` returns the right shape
and dtype for every upstream IMAGE fixture. `NDArray.data_array()`
likewise. Test covers scalar_type values 2 (int8), 3 (uint8), 4
(int16), 10 (float32) at minimum.

### Phase 5 (future / optional) — POLYDATA struct-array flattening

Not in scope for this plan. Recording for future reference:

POLYDATA contains struct arrays like `points: array<{x, y, z: float32}>`.
For a 1M-vertex mesh, the current `list[Point]` representation
allocates 1M Pydantic model instances — seconds, gigabytes.

An optimization: when a struct element consists **entirely of
primitives of the same type**, codegen emits the field as
`np.ndarray` of shape `(N, len(sub_fields))` with the common
dtype, instead of `list[Model]`. User access pattern changes
(`msg.points[i].x` becomes `msg.points[i, 0]`), but the perf
gain is 100-1000× on realistic meshes.

Blocker: user-facing API shift. Either (a) both reps coexist
(slower list of models + faster ndarray helper method), or (b)
we accept the API shift and ship the ndarray form. The schema
doesn't express "this is really geometry" vs "this is really
records," so the codegen heuristic is necessarily imperfect.

Deferred until someone has a real POLYDATA workload asking for it.

## Validation strategy

### Per-phase acceptance tests (already listed above)

### Full-suite verification at end

- All 110 corpus-tools tests pass (reference codec still correct)
- All 126+ core-py tests pass (typed library round-trips 23/24
  fixtures byte-exact)
- All 2 core-cpp ctest entries pass (cross-language parity
  unaffected — C++ side didn't change)
- Bench shows expected speedups for SENSOR.data
- Bench shows no regression for TRANSFORM-class control messages

### Parity verification

The cross-language oracle parity test compares C++ and Python
framing results. This is unaffected: the `ext_header_size`,
`metadata_count`, `round_trip_ok`, etc. flags don't depend on
field representation, only on byte-for-byte body preservation.
Both the C++ and Python codecs still round-trip bytes → bytes
through different intermediate representations.

## File touch list

**Modify:**
- `corpus-tools/src/oigtl_corpus_tools/codec/fields.py` — Phase 1
- `corpus-tools/src/oigtl_corpus_tools/codegen/python_types.py` — Phase 2
- `corpus-tools/src/oigtl_corpus_tools/codegen/templates/python_message.py.jinja` — Phase 2 (imports + validators)
- `corpus-tools/tests/test_codec.py` — Phase 1 (assertion updates)
- `corpus-tools/tests/test_oracle.py` — Phase 1 (assertion updates)
- `core-py/pyproject.toml` — Phase 3 (extra)
- `core-py/README.md` — Phase 3 (docs)
- `core-py/src/oigtl/messages/*.py` — Phase 2 (regenerated)
- `core-py/src/oigtl/messages/__init__.py` — Phase 2 (regenerated)

**Create:**
- `core-py/src/oigtl/runtime/arrays.py` — Phase 2
- `core-py/benches/bench_numpy.py` OR extend `bench_typed.py` — Phase 3
- Codegen hooks for `pixel_array()` / `data_array()` helpers — Phase 4
- `core-py/tests/test_semantic_arrays.py` — Phase 4

**Do not touch:**
- Any `core-cpp/` file (C++ side unchanged, representation concern is Python-only)
- `spec/schemas/*.json` (wire format unchanged, no schema updates needed)
- `corpus-tools/src/oigtl_corpus_tools/schema/` (Pydantic meta-schema unchanged)

## Estimates

| Phase | Scope | Duration |
|---|---|---|
| 1 — Codec bytes path | 20 LoC + test updates | 0.5 days |
| 2 — Runtime helpers + Pydantic validators | ~150 LoC | 1 day |
| 3 — Packaging + bench | ~100 LoC, docs | 0.5 days |
| 4 — Semantic helpers | ~80 LoC + tests | 0.5 days |
| **Total** | **~350 LoC + test updates + docs** | **~2.5 days** |

Phase 5 (struct-array flattening) is out of scope; estimate 1
week standalone if pursued.

## Resuming after compaction

Key facts for the next session:

1. `core-py/` is complete and correct today. All 126 tests pass.
   The work this plan proposes is additive optimization, not
   corrective.
2. The uint8-as-bytes optimization is already shipped
   (commit `9378c96`, `corpus-tools/.../codec/fields.py` around
   line 185). This plan extends that pattern to all primitive types.
3. The bottleneck is `list(struct.unpack_from(">" + fmt*N, ...))`
   in `fields.py`. Replacing with `bytes(data[offset:offset+N*size])`
   is one line; the rest of this plan is rippling that change
   through the typed class and packaging.
4. Pydantic v2 field validators (`@field_validator(mode='before')`)
   are the mechanism for coercing input → canonical runtime type.
5. The `[numpy]` packaging convention follows `polars[numpy]`,
   `pyarrow[numpy]`, etc. Don't use `[array]` — less discoverable.
6. `array.array` type codes: avoid `'l'`/`'L'` (platform-dependent);
   use `'i'`/`'q'` instead.
7. `core-py/tests/test_round_trip_all.py` is the regression safety
   net — if 126 tests still pass after changes, the typed library
   is still correct.
8. If confused about any design choice, `core-cpp/PLAN.md` and
   `core-py/README.md` capture the existing design rationale.

Start with Phase 1 (codec change). Each phase is independently
reviewable and shippable.
