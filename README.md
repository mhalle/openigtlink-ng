# openigtlink-ng

Next-generation OpenIGTLink implementation and specification.

This is a parallel, clean-sheet effort — **not** a fork or branch
of the existing
[openigtlink/OpenIGTLink](https://github.com/openigtlink/OpenIGTLink)
library, which remains the reference v2/v3 implementation in
security-maintenance mode. This project's goal is wire-compatibility
with deployed peers, a modernized and security-audited foundation,
and an additive NG protocol path for new clients.

## Goals

- Wire-compatible with deployed v2/v3 peers (byte-identical where
  behavior overlaps)
- Modern C++17, Python, and TypeScript cores with bounds-checked
  parsing primitives and differential-fuzzed cross-language parity
- Schema-driven message codegen — the spec is the source of truth,
  every core generates from it
- Conformance corpus as the single authority for protocol correctness
- Drop-in source-level replacement for existing upstream C++ consumers
  via a compat shim
- Opt-in NG protocol (v4) as an additive upgrade negotiated via
  `NGHELLO`
- Standards-compliant RTP for video (RFC 6184 / 7798 / 7741)
- TLS, auth, rate limiting, and session policy designed in from day
  one

## Repository layout

Monorepo during early development. Subdirectories are organized by
role; they may split into independent repositories on a future
`openigtlink-ng` GitHub organization when that structural change is
warranted.

- [`spec/`](spec/) — protocol specification, 84 machine-readable
  message schemas, and conformance corpus. The **specification of
  record**.
- [`corpus-tools/`](corpus-tools/) — schema validation, reference
  codec (dict-based, used as the conformance oracle), multi-target
  codegen, and fuzzing harness.
- [`core-py/`](core-py/) — typed Python library (`oigtl` package)
  with async + sync transport (TCP, WebSocket). 84 generated
  Pydantic message classes with one-call typed dispatch.
- [`core-ts/`](core-ts/) — typed TypeScript library
  (`@openigtlink/core` package) with TCP + WebSocket transport
  (client and server). Runs in Node ≥20, browsers, Bun, and Deno.
  Zero runtime dependencies.
- [`core-cpp/`](core-cpp/) — typed C++17 library with TCP transport
  (client and server), built on ASIO. Includes
  [`core-cpp/compat/`](core-cpp/compat/) — a source-compatible shim
  exposing the upstream `igtl::` API so existing consumers (e.g.,
  3D Slicer, PLUS Toolkit) can link against this implementation
  unmodified.
- [`core-c/`](core-c/) — minimal C codec for embedded targets. No
  heap, no transport, no metadata support. Header pack/unpack plus
  generated per-message structs.
- [`security/`](security/) — differential fuzzer, libFuzzer targets,
  negative corpus, and the infrastructure that keeps all four codecs
  byte-identical under adversarial input.

## Status

**Wire codec: done.** All 84 message types from the spec are
implemented in four symmetric codecs (reference Python, typed
Python, typed C++17, typed TypeScript), all cross-checked against
each other and against upstream test fixtures.

**Transport: done for TCP and WebSocket.** Async + sync clients and
servers in all three typed cores; every language-pair combination
is verified by cross-runtime interop tests (py ↔ ts, py ↔ cpp,
ts ↔ cpp).

**Security: four phases complete.** Negative corpus, differential
cross-language fuzzer (millions of iterations, zero disagreements
on the pinned protocol surface), libFuzzer with ASan+UBSan on the
C++ targets, and CI smoke gates on every PR. See
[`security/README.md`](security/README.md).

**TLS, auth, rate limiting, and v4 protocol extensions remain
future work.**

| Layer | Status |
|---|---|
| Message schemas ([`spec/schemas/`](spec/schemas/)) | 84 schemas, fixture round-trip verified |
| Python reference codec ([`corpus-tools/`](corpus-tools/)) | complete |
| Python typed library ([`core-py/`](core-py/)) | complete + TCP + WebSocket transport |
| TypeScript library ([`core-ts/`](core-ts/)) | complete + TCP + WebSocket transport |
| C++17 library ([`core-cpp/`](core-cpp/)) | complete + TCP transport |
| Minimal C codec ([`core-c/`](core-c/)) | runtime + header codec complete; per-message codegen in flight |
| Upstream compat shim ([`core-cpp/compat/`](core-cpp/compat/)) | 60+ header facades, byte-parity tested against upstream |
| Schema-driven codegen (C++, Python, TS, C) | complete, drift-checked in CI |
| Cross-language fuzzing ([`security/`](security/)) | phases 1–4 complete |
| TLS / auth / session policy | not started |
| v4 / NG protocol extensions | not designed |

See each directory's own `README.md` for detailed per-core docs
and usage. Historical implementation plans (`core-*/PLAN.md`) are
kept as decision records; they describe the path taken, not
pending work.

## Documentation

- [`ARCHITECTURE.md`](ARCHITECTURE.md) — how the pieces fit
  together (four language cores, codegen, reference codec,
  fuzzer, compat shim).
- [`CONTRIBUTING.md`](CONTRIBUTING.md) — local build + test
  instructions, the schema-change workflow, coding conventions,
  commit-message style, testing expectations.
- [`SECURITY.md`](SECURITY.md) — threat model, memory-safety
  posture per language core, fuzzer guarantees, vulnerability
  reporting.
- [`spec/TRANSPORT.md`](spec/TRANSPORT.md) — shared transport
  design across the three typed cores (TCP + WebSocket,
  resilience, framing).
- [`spec/CONFORMANCE.md`](spec/CONFORMANCE.md) — how the test
  suite verifies correctness (five layers, from unit tests to
  differential fuzzing to cross-runtime interop).
- Per-core `README.md` — usage and status for each language
  library.

## License

Apache License 2.0. See [`LICENSE`](LICENSE).
