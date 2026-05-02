# spec

Protocol specification, machine-readable message schemas, and the
conformance corpus.

This directory is the **specification of record** for the project.
It contains no implementation code — only the contract that every
implementation must satisfy.

## Layout

```
spec/
├── protocol/         human-readable specification documents
│   ├── v3.md         canonical description of the deployed v2/v3 protocol
│   └── (v4.md)       NG protocol — drafted via RFCs before landing
├── MESSAGES.md       generated message reference (one section per type)
├── schemas/          per-message-type JSON schemas (one file per type)
│   ├── transform.json
│   └── ...
├── corpus/           test vectors (wire bytes + semantic JSON)
│   ├── v3/
│   ├── v4/
│   └── negative/     must-reject inputs
└── meta-schema.json  JSON Schema that validates the message schemas
```

[`MESSAGES.md`](MESSAGES.md) is the human-readable reference for all
84 types — generated deterministically from the schemas by
`oigtl-corpus messages-doc` and kept in sync by CI. Read this if
you want to know what a TRANSFORM looks like; read [`schemas/`](schemas/)
if you want the machine-readable contract.

## Schema format

Each message type has one JSON file under `schemas/` conforming to
[`meta-schema.json`](meta-schema.json). Rather than using comments,
message schemas carry structured metadata fields:

- `description` — concise summary, required on messages and fields;
  flows into every generated language's API documentation.
- `rationale` — optional; explains *why* a design choice was made.
- `spec_reference` — optional; link back to the prose spec document
  (section + URL).
- `introduced_in` — protocol version that first defined the type or field.
- `legacy_notes` — array of strings capturing historical quirks that
  must be preserved for wire compatibility (these are *contract*, not
  incidental documentation).
- `see_also` — cross-references to related message types.

This structure is deliberately richer than comments would be: CI can
enforce presence, doc generators can format it consistently, and audit
tooling can require a `spec_reference` on every field.

## Conformance contract

Any implementation claiming to speak OpenIGTLink must pass every entry
in the corpus. CI for each implementation gates on this.

The corpus is generated against a pinned version of the reference C++
implementation. When implementations disagree with the corpus, either
the implementation is wrong or the corpus needs an update — the
disagreement is resolved explicitly, not silently.

## Status

**Schemas complete.** All 84 wire message types from the v1/v2/v3
protocol are specified under `schemas/` — 22 data messages
(including the legacy `COLORTABLE` alias for `COLORT`), 4
framing structures (header, extended header, metadata, unit), and
58 query/control messages (`GET_*`, `STT_*`, `STP_*`, `RTS_*`).
Every schema has been audited against the upstream reference
implementation and is round-trip verified by all four cores
(reference Python in `../corpus-tools/`, plus the generated
`../core-py/`, `../core-cpp/`, `../core-ts/`, `../core-c/`).

**Corpus complete** for the cases that matter day-to-day. Under
[`corpus/`](corpus/):

- [`upstream-fixtures.json`](corpus/upstream-fixtures.json) — the
  pinned positive corpus (24 fixtures imported byte-for-byte from
  upstream's test tree at the SHA recorded in
  [`ORACLE_VERSION.md`](corpus/ORACLE_VERSION.md)). Every codec's
  CI verifies these round-trip.
- [`negative/`](corpus/negative/) — negative fixtures that every
  codec must reject. Categorised by failure mode (framing,
  metadata, content, …). Pinned regressions surfaced by the
  differential fuzzer live here.

The `v3/` and `v4/` subdirectories are reserved placeholders for a
future generation step that emits per-version positive vectors as
standalone files independent of the upstream test headers — not a
gating dependency for any consumer today.
