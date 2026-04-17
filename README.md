# openigtlink-ng

Next-generation OpenIGTLink implementation and specification.

This is a parallel, clean-sheet effort — **not** a fork or branch of the
existing OpenIGTLink library. The existing implementation at
`../openigtlink/` remains the reference v2/v3 implementation in
security-maintenance mode. This project's goal is wire-compatibility
with that implementation plus a modernized foundation and an additive
NG protocol path for new clients.

## Goals

- Wire-compatible with deployed v2/v3 peers (byte-identical where behavior overlaps)
- Modern C++17 core with bounds-checked parsing primitives
- Schema-driven message codegen
- Conformance corpus as the single source of truth for protocol correctness
- Opt-in NG protocol (v4) as an additive upgrade negotiated via `NGHELLO`
- Standards-compliant RTP for video (RFC 6184 / 7798 / 7741)
- TLS, auth, rate limiting, and session policy designed in from day one
- Eventual native ports in multiple languages, using the same corpus

## Repository layout

Monorepo during early development. Subdirectories are organized by
role; they may split into independent repositories on a future
`openigtlink-ng` GitHub organization when that structural change is
warranted.

- [`spec/`](spec/) — protocol specification, 84 machine-readable
  message schemas, and conformance corpus. The **specification of record**.
- [`corpus-tools/`](corpus-tools/) — schema validation, corpus generation,
  reference codec (dict-based, used as the conformance oracle), and
  multi-target codegen. The schemas' authoritative tooling.
- [`core-cpp/`](core-cpp/) — typed C++17 wire codec + runtime. Round-trips
  23 of 24 upstream fixtures byte-for-byte; cross-language oracle
  parity against the Python reference codec in CI.
- [`core-py/`](core-py/) — typed Python library (`oigtl` package).
  84 generated Pydantic message classes on top of the reference codec,
  with `parse_message()` one-call typed dispatch.
- [`core-ts/`](core-ts/) — typed TypeScript library
  (`@openigtlink/core` package). 84 generated ES-module classes with
  `DataView`-based codec, runs in Node ≥20, browsers, Bun, and Deno.
  Zero runtime dependencies.

## Status

**Wire codec: done.** All 84 message types from the spec are
implemented in four symmetric codecs (reference Python, typed
Python, typed C++17, typed TypeScript), all cross-checked against
each other and against the upstream test fixtures. Transport,
session management, and v4 protocol extensions remain future work.

Current implementation state:

| Layer | Status |
|---|---|
| Message schemas (`spec/schemas/`) | 84 schemas, all fixture round-trip verified |
| Python reference codec (`corpus-tools/`) | complete, 110 tests |
| Python typed library (`core-py/`) | complete, 149 tests |
| C++17 typed library (`core-cpp/`) | complete, 24-fixture corpus + cross-language oracle parity |
| TypeScript library (`core-ts/`) | complete, 103 tests + cross-language parity |
| Schema-driven codegen (C++, Python, TS) | complete, drift-checked in CI |
| Transport layer (ASIO, TLS, session) | not started |
| v4 / NG protocol extensions | not designed |

See [core-cpp/PLAN.md](core-cpp/PLAN.md) for the C++ codec's
phase-by-phase history and acceptance criteria; individual
`README.md` files in each directory describe the current state and
usage.

## License

Apache License 2.0. See [LICENSE](LICENSE).
