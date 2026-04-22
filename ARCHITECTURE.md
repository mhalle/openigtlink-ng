# Architecture

This document explains how the pieces of openigtlink-ng fit
together. It's aimed at contributors and reviewers who need a
mental model before diving into a specific directory.

For what the project is and what it's for, see the
[root README](README.md). For per-core details, see each
directory's own `README.md`.

---

## One-paragraph summary

The [OpenIGTLink protocol spec](spec/) is expressed as 84 JSON
schemas. A Python tool
([`corpus-tools/`](corpus-tools/)) validates schemas and
generates typed codecs for four language cores — Python, TypeScript,
C++, C — from the same schemas. A Python reference codec (dict-based)
lives alongside the generator and acts as the **conformance oracle**:
every other codec is expected to produce byte-identical results
against it, enforced by a differential fuzzer that runs millions of
randomized inputs through all four codecs and fails on any
disagreement. Cross-language integration tests exercise pairwise
client/server interop over TCP and WebSocket. A C++ **compat shim**
([`core-cpp/compat/`](core-cpp/compat/)) re-exposes the upstream
OpenIGTLink API so existing consumers (3D Slicer, PLUS Toolkit) can
link this implementation unmodified.

---

## The five components

### 1. The spec (`spec/`)

The specification of record. Two kinds of artifact:

- **Schemas** (`spec/schemas/*.json`) — one file per message type.
  Each describes field layout, sizes, encodings, and the
  extended-header/metadata rules. A meta-schema
  (`spec/meta-schema.json`) validates all of them. Schemas are
  human-readable and carry prose fields (`description`,
  `rationale`, `spec_reference`, `introduced_in`) that end up in
  generated code as doc comments.
- **Corpus** (`spec/corpus/`) — byte-exact wire test vectors.
  Positive fixtures (must accept + round-trip byte-identical) and
  negative fixtures (must reject). 24 positive upstream fixtures
  are frozen from the reference C++ library; 21 negative fixtures
  pin regressions the differential fuzzer found.

The spec is the contract every other component targets. Changes
to the wire protocol start here.

### 2. The generator and reference codec (`corpus-tools/`)

A Python package (`oigtl-corpus`) that does three things:

- **Schema validation** — confirms every file under
  `spec/schemas/` conforms to the meta-schema.
- **Reference codec** — a dict-based, field-by-field pack/unpack
  implementation used as the conformance oracle. This is not
  optimized for speed; it's optimized for *obvious correctness*.
  When the fuzzer finds a disagreement, we ask "what does the
  reference codec say?" and treat that as ground truth (usually).
- **Multi-target codegen** — Jinja2 templates that emit typed
  Python (Pydantic), typed TypeScript (ES modules), typed C++17
  (struct + pack/unpack), and minimal C (struct + pack/unpack
  for embedded targets). The CLI has a `--check` mode that CI
  uses to detect drift between schemas and generated code.

The reference codec and the generator sit in the same package on
purpose: they share the schema-interpretation logic, and the
generator's output is tested against the reference codec's
behavior as part of its unit tests.

### 3. The language cores

Four independent codec implementations, all targeting the same
spec, held byte-identical by the fuzzer.

- [`core-py/`](core-py/) — typed Python library with async and
  sync TCP + WebSocket transport. Stacks on the reference codec:
  the typed layer (Pydantic classes) wraps the dict-based
  reference. That layering is deliberate — users who want typed
  ergonomics get them; users who want the rawest possible path
  use the reference directly.
- [`core-ts/`](core-ts/) — typed TypeScript with TCP + WebSocket
  client *and* server. Zero runtime dependencies. Runs in
  Node ≥20, modern browsers, Bun, Deno. The server side exists
  because browsers can't open TCP sockets to medical devices, so
  the WebSocket server makes OpenIGTLink addressable from a
  browser.
- [`core-cpp/`](core-cpp/) — typed C++17 with ASIO-based TCP
  client and server. Written to be auditable: small translation
  units, bounds-checked primitives, zero external runtime deps
  beyond stdlib + ASIO. The performance-sensitive core.
- [`core-c/`](core-c/) — minimal C codec for embedded targets
  (ultrasound probes, tracker boxes, IoT bridges). No heap, no
  metadata support, no transport; header pack/unpack plus
  generated per-message structs. Scope is deliberately narrow.

All four cores consume the same schemas and emit the same bytes.
The cross-language fuzzer enforces this; the pairwise interop
tests prove it end-to-end over real transports.

### 4. The security harness (`security/`)

Differential fuzzer, libFuzzer + sanitizer targets, negative
corpus, and the disagreement-logging infrastructure. The fuzzer
generates randomised/mutated wire bytes, feeds them to every
configured oracle (the four codecs plus variants), and fails on
any semantic-level disagreement or any sanitizer report.

Details: [`security/README.md`](security/README.md). Threat
model and disclosure policy: [`SECURITY.md`](SECURITY.md).

### 5. The upstream compat shim (`core-cpp/compat/`)

A C++ source layer that re-exposes the
[upstream OpenIGTLink](https://github.com/openigtlink/OpenIGTLink)
`igtl::` API so consumers built against that library can link
against this implementation without editing their source.
Delegates all codec work to `core-cpp`'s runtime, so users of
the shim transparently get the hardened parsing.

Covers ~95% of the upstream public API; the gap (primarily
around subclass-access to protected members) is enumerated in
[`core-cpp/compat/API_COVERAGE.md`](core-cpp/compat/API_COVERAGE.md)
and, for the specific case of PLUS Toolkit,
[`core-cpp/compat/PLUS_AUDIT.md`](core-cpp/compat/PLUS_AUDIT.md).

---

## How changes flow

### Adding or changing a message type

1. Write or edit a schema under [`spec/schemas/`](spec/schemas/).
2. Run `oigtl-corpus codegen {python,ts,cpp,c}` to regenerate
   every core's typed layer.
3. Add a positive fixture under
   [`spec/corpus/positive/`](spec/corpus/) if cross-language
   verification is needed.
4. All four cores' test suites should pass. The differential
   fuzzer should stay green.
5. CI enforces drift detection via `codegen --check` modes.

See [CONTRIBUTING.md](CONTRIBUTING.md) for the full workflow.

### Fixing a codec bug

1. Write a test that reproduces the bug in the affected core.
2. Fix it.
3. If the bug implies divergence from the reference codec,
   investigate whether the reference is right (update the other
   core) or wrong (update the reference + downstream).
4. If the bug was a security issue (rejection that should have
   fired, silent acceptance of malformed input), add a negative
   fixture under
   [`spec/corpus/negative/`](spec/corpus/negative/) so the same
   input can't re-pass the suite.
5. Run the differential fuzzer locally before pushing.

### Changing transport / framing

Any change to the on-wire framing or transport is load-bearing
and cross-language by definition. The pairwise interop tests
([`core-*/tests/cross_runtime_*`](.)) exercise every
language-pair combination over TCP and WebSocket. A transport
change must keep all pairings green.

---

## Why four codecs?

Reasonable question; let's answer it explicitly.

- **Python reference codec** — the oracle. Deliberately
  un-optimized, deliberately dict-based, deliberately close to
  the schema. Its job is to be obviously correct.
- **Typed Python** — the production Python API. Pydantic types,
  numpy arrays, ergonomic for research code. Built on top of
  the reference.
- **C++** — the performance and deployment target. Many
  OpenIGTLink consumers are C++ applications (Slicer, PLUS) that
  need in-process speed and compatibility.
- **TypeScript** — browsers, web dashboards, Electron apps.
  Also the bridge that lets a browser client speak OpenIGTLink
  via the WebSocket transport.
- **C (minimal)** — embedded targets. Ultrasound probes,
  microcontrollers, devices that can't carry C++ overhead.

The four-way differential fuzzer is the tool that turns this
into a **feature** rather than a maintenance burden. Every codec
keeps every other codec honest.

---

## Design choices worth knowing

### Schema is the source of truth

We do not hand-maintain parallel type definitions across
languages. Codegen is the contract: if you touch a message
type's layout, you touch the schema, not the language-specific
struct. The `--check` CI gate enforces this.

### Generated code is not hand-editable

Every generated file carries a `GENERATED by … — do not edit`
banner. Handwritten runtime code sits next to generated code in
each core and is not banner-marked. Review a diff by checking
the banner first.

### The reference codec as oracle

Using a deliberately un-optimized Python implementation as the
source of truth has been invaluable. Optimizations in the three
typed codecs were written and verified against it; the moments
where we chose to deviate (e.g., the float32 NaN
canonicalization) are documented explicitly. When a fuzzer
disagreement surfaces, the first question is always "what does
the reference say?"

### The compat shim is source-level, not binary-level

We do not try to produce a drop-in `.so` replacement for the
upstream library. We produce headers and a static library that
expose the upstream API; consumers recompile against us. This
is a deliberate choice — binary-level drop-in would force us to
mirror upstream's ABI including its bugs; source-level lets us
swap in safer internals transparently.

### Monorepo for now

The project is structurally a monorepo during early development.
Each subdirectory could become its own repository later; the
boundaries are already drawn to support that split. Cross-
subdirectory references use relative paths within the repo so
they survive any future split.

---

## What you won't find here

- **A single build system.** Each core uses its ecosystem's
  native build tooling (uv for Python, npm for TypeScript,
  CMake for C/C++). There's no top-level bazel/buck/meson that
  drives everything. CI orchestrates the combined build; see
  [`.github/workflows/ci.yml`](.github/workflows/ci.yml).
- **A TLS layer.** Not yet. Transport currently expects to run
  over trusted network segments; TLS + auth + session policy is
  roadmap. See [`SECURITY.md`](SECURITY.md).
- **A v4 / NG protocol implementation.** The wire format
  implemented here is deployed v2/v3, with headroom reserved
  for v4 negotiation (`NGHELLO`). v4 semantics are not yet
  specified.
- **Per-core runtime abstractions.** Each core is written
  idiomatically for its language. The C++ core uses ASIO; the
  Python core uses asyncio; the TypeScript core uses the Node/
  browser networking primitives. There's no shared abstraction
  across them because there couldn't be without being bad in
  all four.
