# Contributing to openigtlink-ng

Thanks for your interest. This document is the entry point for
anyone making a code, schema, spec, or doc change to the project.

For an overview of what the project is and how it's structured, see
the [root README](README.md). For the protocol spec and how the
schema system works, see [`spec/README.md`](spec/README.md).

---

## Before you start

- **License.** Contributions are accepted under the [Apache License
  2.0](LICENSE). By opening a pull request, you agree your
  contribution is licensed under those terms.
- **Open an issue first for non-trivial changes.** Bug fixes and
  small docs changes can go straight to a PR. Anything that touches
  the wire format, adds a new message type, changes cross-language
  behavior, or reshapes a public API benefits from a design
  conversation on an issue before code lands.
- **Security-sensitive changes.** If your change touches codec
  parsing, framing, transport, or the compat shim, read
  [`security/README.md`](security/README.md) first. PRs in those
  areas are held to a higher bar (differential fuzzer + libFuzzer;
  see [Security testing](#security-testing) below).

---

## Repository layout (quick reference)

The repo is a monorepo during early development. Directory roles:

| Directory | What lives there |
| --- | --- |
| [`spec/`](spec/) | Protocol spec, 84 JSON schemas, conformance corpus. **Specification of record.** |
| [`corpus-tools/`](corpus-tools/) | Schema validator, reference codec (Python dict), multi-language codegen, fuzzing harness. |
| [`core-py/`](core-py/) | Typed Python library + transport. |
| [`core-ts/`](core-ts/) | Typed TypeScript library + transport (Node/browser/Bun/Deno). |
| [`core-cpp/`](core-cpp/) | Typed C++17 library + transport + upstream compat shim. |
| [`core-c/`](core-c/) | Minimal C codec for embedded targets (no transport, no heap). |
| [`security/`](security/) | Fuzzing harness, differential oracle, negative corpus. |

See each subdirectory's own `README.md` for detailed per-core docs.

---

## Local development

### Toolchain

- **Python**: [`uv`](https://docs.astral.sh/uv/). PEP 723 for
  single-file scripts; full projects use `uv sync` + `uv run`.
  Required for `core-py/` and `corpus-tools/`.
- **Node.js ≥ 20** (for `core-ts/`).
- **CMake ≥ 3.16** and a C++17 compiler (for `core-cpp/` and
  `core-c/`). CI covers GCC 13, Clang 15, Apple Clang, MSVC 2022.
- **`node` on PATH** is required to run the full cross-runtime
  interop test matrix locally; tests skip gracefully if missing.

### Build + test, per core

```bash
# Python typed library
cd core-py && uv sync --all-extras && uv run pytest

# Corpus tools + reference codec
cd corpus-tools && uv sync && uv run pytest

# TypeScript library
cd core-ts && npm install && npm test

# C++17 library + compat shim
cmake -S core-cpp -B core-cpp/build
cmake --build core-cpp/build
ctest --test-dir core-cpp/build --output-on-failure

# Minimal C codec
cmake -S core-c -B core-c/build
cmake --build core-c/build
ctest --test-dir core-c/build --output-on-failure
```

### Running the cross-language fuzzer locally

Before submitting a PR that touches codec or framing code:

```bash
cd corpus-tools
uv run oigtl-corpus fuzz differential -n 100000 \
    --oracle py-ref --oracle py --oracle cpp --oracle ts
```

Zero disagreements across all four codecs is the bar. CI runs a
50k-iteration smoke version of this on every PR; a full 100k run
locally catches regressions that the smoke run might miss.

---

## Coding conventions

Each language core follows its ecosystem's conventions. Tool
configs are checked into the tree — run the formatter/linter and
your code will pass.

- **Python**: pytest with `asyncio_mode="auto"`. Configs in
  [`core-py/pyproject.toml`](core-py/pyproject.toml) and
  [`corpus-tools/pyproject.toml`](corpus-tools/pyproject.toml).
- **TypeScript**: Biome for format + lint; ESM only; strict
  `tsconfig`. Configs in [`core-ts/biome.json`](core-ts/biome.json)
  and [`core-ts/tsconfig.json`](core-ts/tsconfig.json).
- **C++**: C++17, no extensions, no external runtime deps.
  `-Wall -Wextra -Wpedantic -Werror` on POSIX, `/W4 /WX` on MSVC.
  Settings in [`core-cpp/CMakeLists.txt`](core-cpp/CMakeLists.txt).
- **C**: Embedded-friendly — no heap in the runtime, no stdlib I/O.
  See [`core-c/README.md`](core-c/README.md) for the scope rules.

### Design principles the CI will enforce for you

- **Round-trip equality.** Every generated message type round-trips
  the upstream C++ test fixtures byte-for-byte. Don't break this.
- **Canonical-form stability.** `pack(unpack(bytes))` need not equal
  the input bytes, but must reach a fixed point after one more
  cycle: `pack(unpack(pack(unpack(b)))) == pack(unpack(b))`. See
  [`security/README.md`](security/README.md) for why.
- **No silent acceptance of invalid input.** The negative corpus at
  [`spec/corpus/negative/`](spec/corpus/negative/) lists 21 inputs
  every codec must reject. New parsers must reject them too.
- **Cross-language agreement.** The differential fuzzer compares
  codec outputs across Python (reference + typed), C++, and
  TypeScript. Any disagreement is a bug somewhere; the CI fails.

---

## Commit messages

The project uses **Conventional Commits** with a scope. Look at
`git log --oneline -20` for examples. Format:

```
<type>(<scope>): <subject>

<optional body explaining the why>

Co-Authored-By: <names as applicable>
```

Types in use: `feat`, `fix`, `test`, `docs`, `refactor`, `chore`.

Scopes in use: `core-py`, `core-ts`, `core-cpp`, `core-c`,
`corpus-tools`, `security`, `compat-cpp`, `interop`, `framing`,
`spec`. Add a new scope when a coherent area comes up (e.g., a
new language core).

Keep subjects under ~70 chars. Explain the *why* in the body, not
the *what* (the diff shows the what).

---

## Schema changes (the cascade)

Adding or changing a message type is the one workflow in this repo
that touches almost every directory. The spec is the source of
truth; every core generates from it.

**Workflow:**

1. **Edit or create a schema** under
   [`spec/schemas/<type>.json`](spec/schemas/). Must validate
   against [`spec/meta-schema.json`](spec/meta-schema.json).
2. **Validate the schema:**
   ```bash
   cd corpus-tools
   uv run oigtl-corpus schema validate ../spec/schemas/<type>.json
   ```
3. **Regenerate every language target:**
   ```bash
   uv run oigtl-corpus codegen python
   uv run oigtl-corpus codegen ts
   uv run oigtl-corpus codegen cpp
   uv run oigtl-corpus codegen c
   ```
4. **Add a test fixture** to [`spec/corpus/`](spec/corpus/) if the
   new type needs cross-language verification. Positive fixtures
   go under `spec/corpus/positive/`; must-reject inputs under
   `spec/corpus/negative/`.
5. **Run all four core test suites** and the differential fuzzer
   locally before opening a PR.

**CI enforces drift detection:** every codegen target has a
`--check` mode that fails the build if generated output drifts
from the committed tree. If CI complains, re-run the generator
and commit the delta.

---

## Generated code is generated

Files produced by the codegen carry a banner:

```
// GENERATED by corpus-tools/oigtl-corpus codegen <target> — do not edit.
```

**Do not hand-edit generated files.** Changes will be silently
overwritten on the next codegen run. If you need different
generated output, change the schema, the template, or the
generator code under [`corpus-tools/`](corpus-tools/).

Handwritten runtime code lives alongside the generated code in
each core and is not banner-marked. When in doubt, check for the
banner before editing.

---

## Testing expectations

Different change types carry different test burdens:

| Change type | Minimum tests required |
| --- | --- |
| Typo / docs-only | None beyond existing CI |
| Bug fix in a codec | Regression test reproducing the bug + existing suites pass |
| New message type | Schema + codegen + round-trip test in every core + positive fixture |
| Change to framing / transport | Above + 100k differential fuzzer run clean |
| Change to compat shim | Existing `compat_*` ctest targets stay green + new coverage for added API |
| Schema change affecting wire format | Above + negative-corpus update if new reject cases exist |

CI enforces the baseline; the bar above tells you what a
reasonable reviewer will look for. New tests go next to the code
they cover — each core has a `tests/` directory with clear
precedents.

---

## Security testing

The project's value proposition is security-focused. If your PR
touches anything under:

- [`core-cpp/src/runtime/`](core-cpp/src/runtime/) (framing,
  parsing, metadata handling)
- [`core-cpp/src/transport/`](core-cpp/src/transport/)
- [`core-py/src/oigtl/codec.py`](core-py/src/oigtl/codec.py) or
  [`core-py/src/oigtl/runtime/`](core-py/src/oigtl/runtime/)
- [`core-ts/src/runtime/`](core-ts/src/runtime/) or
  [`core-ts/src/codec.ts`](core-ts/src/codec.ts)
- [`core-cpp/compat/`](core-cpp/compat/) (the upstream shim)

…the following must pass before merge:

- **Differential fuzzer**, 100k iterations, zero disagreements.
- **libFuzzer + ASan + UBSan** on the C++ targets (CI runs a 60s
  smoke; longer runs in the background pipeline).
- **Negative corpus** stays green — no new inputs silently accepted.

See [`security/README.md`](security/README.md) for the full
fuzzing story (what's covered, what bugs have been caught, why
some phases are deferred).

---

## Cross-language parity

The four codecs — Python reference, Python typed, C++ typed,
TypeScript typed — are held byte-identical on the wire. If your
change is in one core, ask yourself:

- Does this change the wire format? → Change must happen in all
  four cores, in the same PR. The differential fuzzer will catch
  any miss.
- Does this change behavior that isn't observable on the wire
  (e.g., an API convenience method)? → Single-core changes are
  fine; note it in the commit body.
- Does this change transport (TCP/WS server/client behavior)? →
  The pairwise interop matrix
  ([`core-*/tests/cross_runtime_*`](.)) gates CI. Any new
  behavior must keep every pairing passing.

---

## Submitting a pull request

1. Fork and branch from `main`. Keep branches focused on one
   coherent change.
2. Before pushing, locally run the test suite for every core your
   change touches, plus the differential fuzzer if applicable.
3. Open the PR with a description that explains the *why*.
   Reference the issue you opened for design discussion if one
   exists.
4. CI must be green. If CI flags a drift-check failure, regenerate
   codegen outputs and commit. If CI flags a fuzz disagreement,
   investigate — that's a real cross-language bug.
5. Reviewers will look for: design soundness, test coverage
   matching the burden table above, commit-message style, and
   docs updates when a public API changed.

Security-sensitive PRs may be held longer for deeper review.
That's expected for a project whose core value prop is
hardening; it's not an indictment of the change.

---

## Reporting bugs and security issues

- **Functional bugs**: open an issue. Minimal reproducer
  appreciated. If the bug is cross-language (e.g., Python accepts
  what C++ rejects), say so — those are treated as high priority.
- **Security vulnerabilities**: see [`SECURITY.md`](SECURITY.md)
  for the disclosure process. Do not open public issues for
  exploitable defects before coordinating a fix.

---

## Questions not answered here?

Check the per-core `README.md`, then open a discussion issue.
This document will grow as conventions crystallize; if you hit a
gap, flagging it is itself a contribution.
