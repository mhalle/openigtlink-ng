# oigtl

Typed Python wire codec for the OpenIGTLink protocol — symmetric to
[`core-cpp`](../core-cpp/), generated from the same schemas under
[`../spec/schemas/`](../spec/schemas/).

## Two flavours of Python codec, one project

This project actually contains two Python codecs:

- **`oigtl_corpus_tools.codec`** in [`../corpus-tools/`](../corpus-tools/)
  — the schema-driven *reference* codec. Walks raw schemas as dicts,
  returns body values as `dict[str, Any]`. Used as the conformance
  oracle and as the implementation underlying this typed library.

- **`oigtl`** (this package) — the *typed* library. Per-message
  Pydantic classes with `pack()` / `unpack()` methods, generated
  from the same schemas. The package downstream applications
  should depend on.

The typed layer sits on top of the reference codec — it doesn't
re-implement field walking. Generated message classes call into
`pack_fields` / `unpack_fields` from corpus-tools and wrap the
result in Pydantic models. Adding a new field shape is a
single-file change in the codegen, not a two-place edit.

## Usage

```python
from oigtl.messages import Transform, Status

# Pack
tx = Transform(matrix=[1.0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0])
body_bytes = tx.pack()

# Unpack
tx = Transform.unpack(body_bytes)
print(tx.matrix)
```

## Status

Scaffold landing now. Per-message codegen for all 84 wire types,
oracle, and dispatch follow in subsequent commits.
