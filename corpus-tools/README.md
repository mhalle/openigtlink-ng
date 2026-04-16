# corpus-tools

Infrastructure that produces and maintains the conformance corpus in
[`../spec/corpus/`](../spec/corpus/).

## Status

Scaffold. Nothing is implemented yet.

## Planned contents

- **Corpus generator** — links the reference C++ implementation
  (hardened fork of the existing OpenIGTLink library), iterates
  canonical exemplars for every message type at every version, and
  writes wire bytes + semantic JSON into `../spec/corpus/`.
- **Schema validator** — checks every `../spec/schemas/*.json` against
  `../spec/meta-schema.json`, plus additional project-specific
  invariants (every field has a `description`, every message has a
  `spec_reference`, etc.).
- **Live-capture pipeline** — consumes `pcap` dumps from real peers
  (3D Slicer, PlusServer, scanner research interfaces) and converts
  them into corpus entries.
- **Differential harness** — runs an input through two implementations
  and reports semantic divergence. Basis for the LLM-assisted fuzz
  triage workflow.
- **libFuzzer / AFL++ harnesses** — structure-aware fuzzing targets
  that feed crashes and divergences back into the corpus as negative
  cases.
- **LLM orchestration** — scripts that take fuzzer output, cluster
  crashes, minimize reproducers, and draft corpus entries for human
  review. The LLM never produces wire bytes directly; bytes go through
  the reference library.

## Language

Probably Python (as glue over pybind11 / PyO3 bindings to the
implementations under test), plus small C++ helpers where needed for
performance or linking reasons.

## License

Apache 2.0. See [`../LICENSE`](../LICENSE).
