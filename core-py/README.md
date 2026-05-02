# oigtl (core-py)

Typed Python wire codec for the OpenIGTLink protocol — symmetric to
[`core-cpp`](../core-cpp/), generated from the same 84 schemas under
[`../spec/schemas/`](../spec/schemas/).

> **Reading order:** this README is quick examples and status. For
> a guided tour of the package's structure (codec / messages / net
> layers), see [`API.md`](API.md). For network usage in detail,
> see [`NET_GUIDE.md`](NET_GUIDE.md). For message-level questions,
> see [`../spec/MESSAGES.md`](../spec/MESSAGES.md).

## Status

**Complete.** 83 generated typed message classes (each round-trips
the upstream fixture byte-for-byte) plus a full async+sync
transport layer (`oigtl.net`) with client, server, resilience,
and accept-time restrictions. 288 tests passing.

## Two Python codecs in one project

This project contains two distinct Python codec layers:

- **`oigtl_corpus_tools.codec`** in [`../corpus-tools/`](../corpus-tools/)
  — the schema-walking *reference* codec. Walks raw schemas as dicts,
  returns body values as `dict[str, Any]`. Used as the conformance
  oracle and as the implementation underlying this typed library.
  Not intended for direct use in applications.

- **`oigtl`** (this package) — the *typed* library. Per-message
  Pydantic classes with `pack()` / `unpack()` methods, generated
  from the same schemas. The package downstream applications
  should depend on.

The typed layer sits on top of the reference codec — it doesn't
re-implement field walking. Generated message classes call into
`pack_fields` / `unpack_fields` from corpus-tools and wrap the
result in Pydantic models.

## Usage

```python
from oigtl.messages import Transform, Status, Image, parse_message
from oigtl.runtime.oracle import verify_wire_bytes

# Construct + pack
tx = Transform(matrix=[1.0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0])
body_bytes = tx.pack()

# Unpack (raises pydantic.ValidationError on bad values)
tx = Transform.unpack(body_bytes)
print(tx.matrix)

# One-call typed dispatch: header parse + CRC verify + framing +
# typed construct (raises ProtocolError subclasses on failure):
msg = parse_message(wire_bytes)
match msg:
    case Transform(matrix=m): ...
    case Status(code=c, error_name=n): ...

# Full oracle: returns a Pydantic VerifyResult with framing summary
# instead of raising. Equivalent in scope to the C++ oracle.
result = verify_wire_bytes(wire_bytes)
if not result.ok:
    print(result.error)

# Semantic helpers for IMAGE / NDARRAY (require the `[numpy]` extra):
from oigtl.semantic import pixel_array, data_array
if isinstance(msg, Image):
    arr = pixel_array(msg)        # ndarray shape (depth, h, w[, C])
    # arr.dtype reflects scalar_type; endian field is honored.
```

## Transport layer — `oigtl.net`

A Python-shaped client and server. Async-native, with a blocking
wrapper for scripts that don't want to learn asyncio.

```python
from oigtl.net import Client, Server, interfaces
from oigtl.messages import Transform, Status

# Minimum viable client
async with await Client.connect("tracker.lab", 18944) as c:
    await c.send(Transform(matrix=[...]))
    reply = await c.receive(Status)

# LAN-only server with host-level restrictions
server = (await Server.listen(18944)) \
    .restrict_to_local_subnet() \
    .set_max_clients(4) \
    .disconnect_if_silent_for(timedelta(minutes=5))

@server.on(Transform)
async def _(env, peer):
    await peer.send(Status(code=1, ...))

await server.serve()
```

See **[NET_GUIDE.md](NET_GUIDE.md)** for the full researcher-
facing reference: resilience (auto-reconnect + offline buffer +
TCP keepalive), the `interfaces` helpers
(`interfaces.primary_address()` etc.), sync wrappers, and the
error model.

## Array representation

Variable-count primitive array fields (`SENSOR.data`,
`POSITION.quaternion`, `NDARRAY.size`, ...) are carried at the
user-facing type layer as:

| Wire element | With `[numpy]` | Without `[numpy]` |
|---|---|---|
| `uint8` | `bytes` | `bytes` |
| fixed-count non-`uint8` | `list[T]` | `list[T]` |
| variable-count non-`uint8` | `np.ndarray` (dtype=`>f4`/`>i2`/…) | `array.array` (native-endian) |

Constructor accepts any of `bytes`, `list`, `tuple`, `array.array`,
or `np.ndarray` — a Pydantic validator coerces to the canonical
form. `pack()` accepts all shapes and produces the correct wire
bytes.

## Install + test

```bash
cd core-py
uv sync --all-extras      # includes [numpy] for full test coverage
uv run pytest
```

All 149 tests should pass in under half a second. Without
`--all-extras` the runtime falls back to `array.array` and the
semantic helper tests are skipped.

For end users:

```bash
pip install oigtl              # stdlib-only (array.array fallback)
pip install 'oigtl[numpy]'     # recommended for imaging / sensor workloads
```

## Performance

Release CPython 3.12 on Apple Silicon. "ref" = dict codec from
corpus-tools, "typed" = Pydantic classes from this package.

| Operation | ref (dict) | typed (Pydantic) |
|---|---|---|
| TRANSFORM unpack | 0.56 µs | 1.43 µs |
| TRANSFORM pack | 0.49 µs | 1.25 µs |
| IMAGE 50×50 unpack | 9.7 µs | 11 µs |
| IMAGE 640×480 unpack | 9 µs | 11 µs |
| IMAGE 1920×1080 unpack | 128 µs | 135 µs |
| IMAGE 1920×1080 `parse_message` (full pipeline) | — | 5.66 ms |
| `model_construct` (skip validation) | — | ~2 µs regardless of size |

Key facts:

- **uint8 arrays are typed as `bytes`** (not `list[int]`). One
  allocation instead of N Python int objects; Pydantic validation
  is a length check, not per-element. Typed unpack on a 2 MB FHD
  grayscale image is 135 µs, not 22 ms.
- **CRC-64 is the dominant cost** of `parse_message` on large
  bodies. The codec uses the `crcmod` C extension (~400 MB/s)
  automatically; pure-Python fallback is ~6× slower. `crcmod` is
  a hard runtime dep.
- **For trusted sources, `Image.model_construct(**values)` +
  `check_crc=False`** drops FHD parsing to ~10 µs, making it
  competitive with the C++ path for validation-free scenarios.

Benchmark script:

```bash
cd core-py
uv run python benches/bench_typed.py
```

## Regenerating

The `src/oigtl/messages/*.py` files and `__init__.py` are generated.
To regenerate after a schema change:

```bash
uv run --project corpus-tools oigtl-corpus codegen python
```

CI enforces drift via `oigtl-corpus codegen python --check`.

## Dependencies

- [`pydantic`](https://pydantic.dev) ≥ 2.5 — typed message classes.
- [`crcmod`](https://pypi.org/project/crcmod/) ≥ 1.7 — C-extension
  CRC-64 fast path. Hard runtime dep because parse_message latency
  on multi-KB bodies is dominated by CRC.
- [`numpy`](https://numpy.org) ≥ 1.24 — **optional** `[numpy]` extra.
  Enables ndarray-valued variable-count primitive fields and the
  `oigtl.semantic.pixel_array` / `data_array` helpers. Follows the
  convention used by `polars[numpy]` / `pyarrow[numpy]`. Without it
  the library falls back to stdlib `array.array` — correct but
  measurably slower on bulk float arrays.
- `oigtl-corpus-tools` — sibling package, provides the codec
  primitives (pack_fields / unpack_fields / header). Depended on
  as an editable path install during monorepo development.

## License

Apache 2.0. See [`../LICENSE`](../LICENSE).
