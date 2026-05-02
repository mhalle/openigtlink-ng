# openigtlink-ng

**A modernization of the OpenIGTLink library and API.**

openigtlink-ng is *not* a fork of
[openigtlink/OpenIGTLink](https://github.com/openigtlink/OpenIGTLink)
and *not* a new protocol. It is a clean-room implementation of the
existing OpenIGTLink v2/v3 protocol — same wire format, same message
types, same semantics — with the surrounding library, API, and
tooling rebuilt under a modern, security-audited design.

If you currently use the upstream `libOpenIGTLink` C++ library and
think of *that library* as "OpenIGTLink", the distinction this
project makes is worth keeping in mind:

- **The protocol** — formally described in [`spec/`](spec/), wire
  format only, language- and library-independent.
- **An implementation** — upstream's library is one. This project is
  another. Both speak the same bytes.

## Where to start

| You are… | Start here |
|---|---|
| Porting an existing C++ application written against upstream's `igtl::` API | [`core-cpp/compat/MIGRATION.md`](core-cpp/compat/MIGRATION.md) |
| Porting PLUS Toolkit specifically | [`core-cpp/compat/PORTING_PLUS.md`](core-cpp/compat/PORTING_PLUS.md) |
| Writing new C++ code | [`core-cpp/API.md`](core-cpp/API.md) → [`core-cpp/CLIENT_GUIDE.md`](core-cpp/CLIENT_GUIDE.md) |
| Writing Python (research scripts, dashboards) | [`core-py/API.md`](core-py/API.md) → [`core-py/NET_GUIDE.md`](core-py/NET_GUIDE.md) |
| Writing TypeScript (browser dashboards, Node services) | [`core-ts/API.md`](core-ts/API.md) |
| Working on an embedded target (MCU, IoT bridge) | [`core-c/API.md`](core-c/API.md) |
| Looking up what's in a TRANSFORM / IMAGE / etc. | [`spec/MESSAGES.md`](spec/MESSAGES.md) |
| Auditing the wire protocol | [`spec/protocol/v3.md`](spec/protocol/v3.md) |
| Reviewing security posture / threat model | [`SECURITY.md`](SECURITY.md), [`security/`](security/) |
| Contributing | [`CONTRIBUTING.md`](CONTRIBUTING.md), [`ARCHITECTURE.md`](ARCHITECTURE.md) |

## What it is

This repository contains:

- **A specification and testing core for OpenIGTLink v2/v3.** 84
  machine-readable message schemas under [`spec/schemas/`](spec/schemas/),
  a meta-schema that validates them, and a conformance corpus of
  byte-exact wire vectors. The deployed protocol exists; we are
  writing it down formally for the first time.
- **A reference implementation.** A deliberately un-optimized,
  dict-based Python codec ([`corpus-tools/`](corpus-tools/)) used as
  the conformance oracle. Every other codec in the project is
  differentially fuzzed against it; when codecs disagree, the
  reference is consulted first.
- **A C++ library with two API surfaces** ([`core-cpp/`](core-cpp/)).
  A modern `oigtl::` API for new code, and a source-compatible
  `igtl::` shim ([`core-cpp/compat/`](core-cpp/compat/)) that lets
  existing applications (3D Slicer, PLUS Toolkit, lab tracking
  servers) recompile against this implementation without source
  changes.
- **A complete new Python library** ([`core-py/`](core-py/),
  `oigtl` package). Typed messages, async + sync TCP and WebSocket
  transport, idiomatic for research code.
- **A new TypeScript library** ([`core-ts/`](core-ts/),
  `@openigtlink/core`). TCP and WebSocket, client and server. Runs
  in Node ≥20, modern browsers, Bun, and Deno — so a browser can
  speak OpenIGTLink directly.
- **A minimal C codec** ([`core-c/`](core-c/)) for embedded targets
  where heap allocation and v3 metadata are too expensive.
- **A security harness** ([`security/`](security/)). A differential
  cross-language fuzzer, libFuzzer + sanitizer targets, and a
  negative corpus. Keeps all four codecs byte-identical under
  randomized adversarial input.

## What it doesn't do

- **It doesn't change the protocol.** Bytes on the wire are
  byte-identical to what upstream's library emits. A peer running
  `libOpenIGTLink` and a peer running openigtlink-ng cannot tell
  each other apart. CI verifies this against upstream's own test
  fixtures on every PR.
- **It doesn't deprecate upstream.** Upstream remains the reference
  v2/v3 implementation in security-maintenance mode; this project
  complements rather than replaces it.
- **It doesn't require you to rewrite anything.** Existing C++ code
  linked against `-lOpenIGTLink` recompiles against `-loigtl` with
  the compat shim — no source edits. The shim covers ~95% of
  upstream's public API; see
  [`core-cpp/compat/MIGRATION.md`](core-cpp/compat/MIGRATION.md).

The v4 / "NG" protocol track is a separate, additive design
negotiated via `NGHELLO`. It is not implemented yet and will be
opt-in when it is. The v2/v3 layer described above is unaffected.

## Design principles

How the project is built, beyond what it is and what it does:

- **Schema-driven codegen.** The spec is the source of truth. Every
  codec is generated from [`spec/schemas/`](spec/schemas/); changing
  a message means changing its schema and regenerating every core.
  CI gates on drift.
- **Conformance corpus as the single authority.** When codecs
  disagree, the corpus decides — not a privileged "main"
  implementation. The reference Python codec exists to be obviously
  correct, not fast.
- **Differential fuzzing across all four codecs.** Randomized and
  mutated inputs feed every codec on every PR; any disagreement is
  treated as a bug, full stop. Millions of iterations to date with
  zero outstanding disagreements on the pinned protocol surface.
- **Source-level, not binary, compat.** Your C++ code recompiles
  against headers we control and runs through our hardened parsing
  path. We do not try to be a binary drop-in for upstream's `.so`,
  which would force us to mirror upstream's ABI and internals
  (including bugs). The trade-off is that you recompile; we believe
  it is the right side of the trade.
- **Security designed in from day one.** Threat model, bounds-checked
  parsing primitives, the negative-corpus contract, and the
  fuzzer/sanitizer infrastructure are present today. TLS, auth,
  rate limiting, and session policy are roadmapped (see Status); the
  scaffolding to land them safely is already in place.

## Repository layout

Monorepo during early development. Each directory's `README.md`
covers usage and per-component status. Quick reference:

| Directory | Role |
|---|---|
| [`spec/`](spec/) | Protocol specification, 84 schemas, conformance corpus. **Specification of record.** |
| [`corpus-tools/`](corpus-tools/) | Schema validation, reference codec (oracle), multi-target codegen, fuzzing harness. |
| [`core-py/`](core-py/) | Typed Python library (`oigtl`) — async + sync TCP/WebSocket. |
| [`core-ts/`](core-ts/) | Typed TypeScript library (`@openigtlink/core`) — TCP + WebSocket, Node + browsers + Bun + Deno, zero deps. |
| [`core-cpp/`](core-cpp/) | Typed C++17 library — TCP, ASIO. Includes [`compat/`](core-cpp/compat/), the source-level `igtl::` shim. |
| [`core-c/`](core-c/) | Minimal C codec for embedded targets. No heap, no metadata, no transport. |
| [`security/`](security/) | Differential fuzzer, libFuzzer targets, negative corpus. |

(Subdirectories may split into independent repositories on a future
`openigtlink-ng` GitHub organization when that becomes useful.)

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

**Future work.** TLS, authentication, rate limiting, and session
policy are roadmapped — designed in from the start, not yet
implemented. Standards-compliant RTP for video (RFC 6184 / 7798 /
7741) is on the same track. The v4 / NG protocol extensions
([`spec/protocol/`](spec/protocol/)) are not yet specified beyond
the `NGHELLO` negotiation hook reserved in v3.

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
and usage.

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
- [`spec/MESSAGES.md`](spec/MESSAGES.md) — message reference for
  all 84 types: fields, semantics, version applicability, legacy
  notes. Generated from the schemas; kept in sync by CI.
- Per-language API tours — guided structural overview of each
  core's package, sitting between the README's quick examples and
  per-symbol reference.
  [`core-py/API.md`](core-py/API.md) ·
  [`core-cpp/API.md`](core-cpp/API.md) ·
  [`core-ts/API.md`](core-ts/API.md) ·
  [`core-c/API.md`](core-c/API.md)
- [`core-cpp/compat/MIGRATION.md`](core-cpp/compat/MIGRATION.md) —
  swapping `libOpenIGTLink` for openigtlink-ng in an existing
  C++ project. Linker flags, build recipes, FAQ.
- [`core-cpp/compat/PORTING_PLUS.md`](core-cpp/compat/PORTING_PLUS.md) —
  end-to-end porting guide for PLUS Toolkit, with verification
  and troubleshooting.
- [`CHANGELOG.md`](CHANGELOG.md) — versioning policy and
  change history.
- Per-core `README.md` — usage and status for each language
  library.

## License

Apache License 2.0. See [`LICENSE`](LICENSE).
