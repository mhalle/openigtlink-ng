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

3. **Codegen** — Jinja2-based rendering of typed C++17 and Python
   message classes from the schemas. Exposed as `codegen cpp` and
   `codegen python`. Both have drift-check modes for CI.

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
