# corpus-tools

Tooling for producing and maintaining the conformance corpus at
[`../spec/corpus/`](../spec/corpus/).

Python module with a single `oigtl-corpus` command-line entry point and a
command / subcommand structure. All Python work in the project uses
[`uv`](https://github.com/astral-sh/uv); there is no `pip install` path.

## Installation and first run

```bash
cd corpus-tools
uv sync --extra dev    # installs the project + test dependencies
```

After `uv sync`, subsequent invocations reuse the project's virtual
environment automatically.

## CLI

```
oigtl-corpus <command> <subcommand> [options]
```

### `schema validate`

Validates message schemas under `../spec/schemas/*.json` against the
Pydantic `MessageSchema` model in
[`src/oigtl_corpus_tools/schema/model.py`](src/oigtl_corpus_tools/schema/model.py)
— the source of truth. The JSON Schema at `../spec/meta-schema.json`
is a derived artifact; this command does not consult it.

```bash
# Validate every schema in the repo
uv run oigtl-corpus schema validate

# Validate specific files
uv run oigtl-corpus schema validate ../spec/schemas/transform.json
```

Exit codes: `0` if all schemas pass, `1` if any fail validation, `2` on
setup errors (repo root not found, etc.).

On failure, each violating JSON pointer is printed with the validation
error message — all errors for all files in a single pass rather than
stopping at the first.

### `schema emit-meta`

Regenerates `../spec/meta-schema.json` from the Pydantic models, or
verifies it is in sync with `--check`.

```bash
# Regenerate spec/meta-schema.json in place
uv run oigtl-corpus schema emit-meta

# Verify the checked-in file is in sync (CI-friendly, does not write)
uv run oigtl-corpus schema emit-meta --check
```

The JSON Schema exists for non-Python consumers (editors, external
reviewers, codegen in other languages). The Python models are
authoritative; any edit to the JSON Schema by hand is reverted by the
next `emit-meta` run. The test `test_meta_schema_on_disk_in_sync_with_models`
enforces sync in CI.

## Tests

```bash
cd corpus-tools
uv run pytest
```

Tests live in `tests/`. Coverage currently includes:

- The repository's real schemas validate against the real meta-schema.
- Synthetic schemas exercise the validator's required-field,
  field-name-pattern, and unknown-property rejection paths.

## Planned subcommands

Listed to make the intended scope visible. None are implemented yet.

- `reference sync` — re-clone / re-verify the reference libraries under
  `reference-libs/` against the pins in `../spec/corpus/ORACLE_VERSION.md`.
- `reference build` — compile the reference libraries into static
  objects the generator and differential harness can link.
- `corpus generate` — produce positive corpus entries by running every
  message-type schema's canonical exemplars through the patched
  reference library. Requires patched oracle to be pinned.
- `corpus differential` — run existing corpus entries through both
  reference libraries and flag semantic divergences.
- `fuzz run` — drive libFuzzer / AFL++ campaigns, with LLM-assisted
  triage of minimized reproducers into the negative corpus.

## Project layout

```
corpus-tools/
├── pyproject.toml
├── src/
│   └── oigtl_corpus_tools/
│       ├── __init__.py
│       ├── __main__.py                 # python -m oigtl_corpus_tools entry
│       ├── cli.py                      # argparse dispatcher
│       ├── paths.py                    # repo-root / spec-path helpers
│       └── commands/
│           ├── __init__.py
│           └── schema.py               # "schema" subcommand
├── tests/
│   ├── __init__.py
│   └── test_schema_validate.py
└── reference-libs/                     # gitignored; see reference-libs/README.md
    └── README.md
```

## License

Apache 2.0. See [`../LICENSE`](../LICENSE).
