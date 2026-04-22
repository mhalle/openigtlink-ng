# corpus-tools

Python tooling for the openigtlink-ng project. Three roles:

1. **Schema validation and meta-schema emission** — the Pydantic
   models in `schema/` describe what a schema file may legally say;
   the JSON Schema at `../spec/meta-schema.json` is generated from
   them. Both the models and the schema files themselves get
   validated by the `schema` subcommand.

2. **Reference codec and oracle** — `codec/` is a schema-walking,
   dict-based Python codec used as the conformance oracle. It's
   the ground truth that typed implementations (`../core-cpp/`,
   `../core-py/`) are cross-checked against. Exposed as a CLI via
   the `oracle` subcommand.

3. **Codegen** — Jinja2-based rendering of typed C++17, Python,
   and TypeScript message classes from the schemas. Exposed as
   `codegen cpp`, `codegen python`, and `codegen ts`. All have
   drift-check modes for CI.

4. **Fixture export** — one-shot JSON dump of the upstream test
   fixtures so non-Python implementations (core-ts, hypothetical
   future Rust/Go ports) consume the same data without needing the
   Python `.h` extractor. Exposed as `fixtures export-json`.

Python module with a single `oigtl-corpus` console script and a
command / subcommand structure. All Python work in the project uses
[`uv`](https://github.com/astral-sh/uv); there is no `pip install`
path.

## Install + test

```bash
cd corpus-tools
uv sync
uv run pytest     # 110 tests
```

## CLI reference

```
oigtl-corpus <command> <subcommand> [options]
```

### `schema validate`

Validates schemas under `../spec/schemas/*.json` against the
Pydantic `MessageSchema` model in `src/oigtl_corpus_tools/schema/` —
the source of truth. The JSON Schema at `../spec/meta-schema.json`
is a derived artifact; this command does not consult it.

```bash
uv run oigtl-corpus schema validate                         # all schemas
uv run oigtl-corpus schema validate ../spec/schemas/transform.json
```

Exit: `0` on success, `1` on validation failure, `2` on setup errors.

### `schema emit-meta`

Regenerates `../spec/meta-schema.json` from the Pydantic models, or
verifies it's in sync with `--check`.

```bash
uv run oigtl-corpus schema emit-meta           # regenerate
uv run oigtl-corpus schema emit-meta --check   # CI-friendly drift guard
```

### `codegen cpp`

Emits typed C++17 message classes from the schemas into
`../core-cpp/include/oigtl/messages/` and
`../core-cpp/src/messages/`, plus a `register_all.{hpp,cpp}` pair
that populates a dispatch registry with all 84 types.

```bash
uv run oigtl-corpus codegen cpp                         # regenerate all
uv run oigtl-corpus codegen cpp --type-id TRANSFORM     # subset
uv run oigtl-corpus codegen cpp --check                 # CI drift guard
```

Every generated file carries a `GENERATED ... do not edit` banner.
The drift guard in CI is what keeps hand-edits from silently
replacing codegen output.

### `codegen python`

Emits typed Python (Pydantic) message classes into
`../core-py/src/oigtl/messages/`, plus an `__init__.py` that
re-exports them and provides `REGISTRY` (type_id → class). Same
flags as `codegen cpp`.

```bash
uv run oigtl-corpus codegen python            # regenerate all
uv run oigtl-corpus codegen python --check    # CI drift guard
```

### `codegen ts`

Emits typed TypeScript message classes into
`../core-ts/src/messages/`, plus an `index.ts` that re-exports
them and registers them with the runtime dispatcher. Each class
is an ESM module with `unpack(bytes)` / `pack()` methods using
the `DataView`-based big-endian helpers from
`../core-ts/src/runtime/byte_order.ts`.

```bash
uv run oigtl-corpus codegen ts                # regenerate all
uv run oigtl-corpus codegen ts --check        # CI drift guard
```

### `fixtures export-json`

Serializes every upstream test fixture (`UPSTREAM_VECTORS`) to
`../spec/corpus/upstream-fixtures.json`. Consumed by `core-ts`
tests and available to any non-Python implementation that wants
the same conformance data without porting the `.h` extractor.

```bash
uv run oigtl-corpus fixtures export-json          # regenerate
uv run oigtl-corpus fixtures export-json --check  # CI drift guard
```

### `oracle verify`

Runs the conformance oracle on wire bytes and emits a JSON report.
Primary consumer is `../core-cpp/tests/oracle_parity_test.cpp`,
which feeds every fixture through both sides and asserts they
agree. Also useful for ad-hoc inspection of a suspect capture.

```bash
uv run oigtl-corpus oracle list-fixtures              # enumerate fixtures
uv run oigtl-corpus oracle verify --fixture transform # decode a known fixture
uv run oigtl-corpus oracle verify --hex 00014e4444... # decode a hex literal
cat some.igtl | uv run oigtl-corpus oracle verify --stdin
```

Output shape (stable):

```json
{
  "ok": true,
  "type_id": "TRANSFORM",
  "device_name": "DeviceName",
  "version": 1,
  "body_size": 48,
  "ext_header_size": null,
  "metadata_count": 0,
  "round_trip_ok": true,
  "error": ""
}
```

## Walkthrough: adding a new message type

Concrete end-to-end example for the most common contribution to
this tree. Say you want to add a `HEARTRATE` message that
carries one `uint16` beats-per-minute value and a 4-character
ASCII sensor-id string.

### 1. Write the schema

Create `../spec/schemas/heartrate.json`:

```json
{
  "$schema": "../meta-schema.json",

  "message_type": "HEARTRATE",
  "type_id": "HEARTRATE",
  "introduced_in": "v2",

  "description": "Instantaneous heart rate from a pulse-oximetry or ECG-derived sensor.",
  "rationale": "Demonstration schema for the corpus-tools walkthrough; not part of the deployed OpenIGTLink spec.",

  "body_size": 6,

  "fields": [
    {
      "name": "bpm",
      "type": "uint16",
      "size_bytes": 2,
      "description": "Beats per minute, integer. 0 indicates no valid reading."
    },
    {
      "name": "sensor_id",
      "type": "fixed_string",
      "encoding": "ascii",
      "size_bytes": 4,
      "description": "4-character ASCII sensor identifier, space-padded."
    }
  ],

  "metadata_allowed": true
}
```

Match the structure of an existing schema (`transform.json`,
`status.json`, `string.json`) to see the full field vocabulary.
The meta-schema at `../spec/meta-schema.json` is the formal
contract; pydantic validates against it.

### 2. Validate the schema

```bash
cd corpus-tools
uv run oigtl-corpus schema validate ../spec/schemas/heartrate.json
```

This confirms the schema is well-formed and the field sizes
add up to `body_size`. Fix any reported errors before
continuing.

### 3. Regenerate every language core

```bash
uv run oigtl-corpus codegen python
uv run oigtl-corpus codegen ts
uv run oigtl-corpus codegen cpp
uv run oigtl-corpus codegen c
```

Each target writes generated files into its respective core
(e.g., `core-py/src/oigtl/messages/heartrate.py`). Every
generated file carries a `GENERATED … do not edit` banner.

### 4. Check that each core's tests still pass

```bash
cd ../core-py && uv run pytest
cd ../core-ts && npm test
cmake --build ../core-cpp/build && ctest --test-dir ../core-cpp/build
cmake --build ../core-c/build && ctest --test-dir ../core-c/build
```

The generated message class is automatically picked up by each
core's registry; no hand-wiring.

### 5. (Optional but recommended) Add a fixture

If you want cross-language conformance proof — and you do —
construct a byte-exact wire blob and add it to the positive
corpus. See [`../spec/CONFORMANCE.md`](../spec/CONFORMANCE.md)
for the full conformance-testing story.

### 6. Run the differential fuzzer

```bash
cd corpus-tools
uv run oigtl-corpus fuzz differential -n 100000 \
    --oracle py-ref --oracle py --oracle cpp --oracle ts
```

Should stay at zero disagreements. If not, you've uncovered a
schema-interpretation divergence between two language targets'
codegen — investigate before shipping.

### 7. Commit

Per [`../CONTRIBUTING.md`](../CONTRIBUTING.md), the commit should
include the schema, the generated outputs from all four cores,
and the fixture if you added one. A Conventional Commit message
like `feat(spec): add HEARTRATE message type` captures the change
type and scope.

---

## Project layout

```
corpus-tools/
├── pyproject.toml
├── src/oigtl_corpus_tools/
│   ├── cli.py                     argparse dispatcher
│   ├── paths.py                   repo-root / spec-path helpers
│   ├── schema/                    Pydantic meta-schema models
│   │   ├── types.py                  enums + aliases
│   │   ├── element.py                ElementDescriptor (struct elements)
│   │   ├── field.py                  FieldSchema (the richest model)
│   │   ├── message.py                MessageSchema (root)
│   │   └── emit.py                   meta-schema JSON generator
│   ├── codec/                     reference codec (conformance oracle)
│   │   ├── primitives.py, fields.py, header.py, crc64.py
│   │   ├── message.py                load_schema, unpack_message, pack_message
│   │   ├── oracle.py                 verify_wire_bytes (framing-aware)
│   │   └── test_vectors.py           extracts upstream .h fixtures
│   ├── codegen/                   Jinja-based source emitters
│   │   ├── cpp_types.py / cpp_emit.py        C++ target
│   │   ├── python_types.py / python_emit.py  Python target
│   │   └── templates/                        .jinja templates
│   └── commands/                  one file per CLI command
│       ├── schema.py, codegen.py, oracle.py
├── tests/                         110 pytest tests
└── reference-libs/                gitignored upstream for fixtures/parity
```

## Planned subcommands (not yet implemented)

- `reference sync` / `reference build` — re-clone and compile the
  reference libraries pinned in `../spec/corpus/ORACLE_VERSION.md`.
- `corpus generate` — produce positive corpus entries by running
  every message type's exemplars through the pinned oracle.
- `corpus differential` — run existing corpus entries through
  multiple reference libraries and flag semantic divergences.
- `fuzz run` — drive libFuzzer / AFL++ campaigns.

None block any of the currently shipping functionality.

## License

Apache 2.0. See [`../LICENSE`](../LICENSE).
