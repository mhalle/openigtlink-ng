# Changelog

All notable changes to openigtlink-ng are recorded here.

The format is loosely based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the
project aspires to follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html) once
releases begin.

---

## Versioning policy

The project is **pre-1.0** and currently has no tagged releases.
All development happens on `main`; consumers pinning a SHA is
the only stable-reference mechanism today.

When releases begin:

- **Major versions** bump on breaking changes to the wire
  format, the public API of any language core, or the compat
  shim's supported upstream API.
- **Minor versions** bump on additive features (new message
  types, new transport, new core).
- **Patch versions** bump on fixes that preserve both wire-
  format and API compatibility.

The wire format is the invariant we hold most tightly: a v2/v3
frame produced by any version of this project must parse
identically in any other version. Security fixes may close
previously-too-permissive parser paths (e.g., the negative
corpus tightens over time), which is a deliberate exception to
strict compatibility.

---

## [Unreleased]

Development is ongoing. Substantive changes since the project
began track in the git log; see `git log --oneline main` for
the full record. High-level themes of recent work:

### Added

- Four-way cross-language codec: reference Python, typed
  Python, typed C++17, typed TypeScript (all 84 message types).
- Minimal C codec for embedded targets (runtime + header
  codec; per-message codegen in flight).
- Transport layer: async + sync TCP client and server in
  Python; TCP client and server in C++17; TCP + WebSocket
  client and server in TypeScript.
- Cross-runtime interop tests spanning every language pair
  that shares a transport.
- Upstream compat shim (`core-cpp/compat/`) exposing the
  `igtl::` API for drop-in replacement of upstream
  OpenIGTLink C++ library in existing consumers.
- Differential fuzzing harness, negative corpus (21 pinned
  must-reject inputs), libFuzzer + ASan/UBSan gates in CI.
- Public codec API (`unpack_*` / `pack_*`) and message
  registry with extension points in all three typed cores.
- PLUS Toolkit audit machinery: pinned SHA, compile-time
  header-surface proof, documented remaining patches needed
  for full drop-in.
- Top-level documentation: `ARCHITECTURE.md`,
  `CONTRIBUTING.md`, `SECURITY.md`, `spec/TRANSPORT.md`,
  `spec/CONFORMANCE.md`.

### Fixed

- v2 framing bug: send sites declared `version=2` on the wire
  but emitted bare body content rather than the 12-byte
  extended header + content. Fixed in three layers: send
  sites downgraded to `version=1` for v1-only paths, a
  `pack_header` invariant check that rejects mismatched
  bodies at pack time, and a `receive` path that strips the
  extended header before handing content to typed unpack.
  Discovered by the py ↔ cpp interop test.
- C++ `parse_wire` integer overflow on malformed 58-byte
  headers (size_t wrap on `kHeaderSize + body_size` near
  `UINT64_MAX`). Caught on the first 30-second libFuzzer
  run. Fixed; regression pinned in the negative corpus.
- Cross-language ASCII strictness unified (non-ASCII rejected
  at unpack in header + body string fields across all four
  codecs).
- Length-prefixed string bounds check in Python (was silently
  truncating; now rejects out-of-bounds lengths).
- Round-trip canonicalization rule for float32 signaling
  NaNs: different runtimes normalize them differently, so
  codecs now accept canonical-form equivalence
  (`pack(unpack(pack(unpack(b)))) == pack(unpack(b))`) as
  sufficient for the round-trip property.
- TypeScript NUL-padded string parsing aligned with Python/
  C++ (split on first NUL, not strip trailing).

### Security

- Differential fuzzer runs clean at 1M iterations across all
  four codecs. 200k iterations additionally exercise the
  stdlib-only (no numpy) Python fallback path — also clean.
- libFuzzer runs clean at 60 s for `fuzz_header` and
  `fuzz_oracle` under ASan + UBSan in CI on every PR.
- Negative corpus pins 21 regressions; extended whenever a
  new rejection bug is surfaced.

See [`SECURITY.md`](SECURITY.md) for the threat model and
[`security/README.md`](security/README.md) for the detailed
fuzzer story.

---

## Entry conventions

Future entries under a versioned release heading should group
changes under these sections (add or omit as needed):

- **Added** — new features or capabilities.
- **Changed** — existing functionality that changed shape
  without breaking compatibility.
- **Deprecated** — features still present but scheduled for
  removal.
- **Removed** — features that have been taken out.
- **Fixed** — bug fixes (include repro notes or corpus
  pointers if applicable).
- **Security** — vulnerability fixes, coverage expansions,
  posture changes. Link to the negative-corpus entry if a
  regression pin was added.

Commit messages already follow Conventional Commits with
scope; a release entry summarises them, it doesn't duplicate
them.
