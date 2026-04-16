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
├── schemas/          per-message-type JSON schemas (one file per type)
│   ├── transform.json
│   └── ...
├── corpus/           test vectors (wire bytes + semantic JSON)
│   ├── v3/
│   ├── v4/
│   └── negative/     must-reject inputs
└── meta-schema.json  JSON Schema that validates the message schemas
```

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

Draft. The prose spec in `protocol/v3.md` and example schema
`schemas/transform.json` are initial scaffolding; the full corpus will
be generated from the hardened existing library once
`../corpus-tools/` is implemented.
