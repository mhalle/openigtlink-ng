# Security Policy

openigtlink-ng exists because the OpenIGTLink protocol is widely
deployed in clinical and research contexts — surgical navigation,
image-guided therapy, real-time tracking — where the attacker
model is realistic and the consequences of silent parsing bugs
are material. Security is not a peripheral feature; it's the
reason the project exists.

This document describes what the project guarantees today, what
it deliberately does not yet guarantee, and how to report a
suspected vulnerability.

---

## Threat model

The project defends against:

- **A malicious or malfunctioning peer on the wire.** Any
  implementation that accepts arbitrary OpenIGTLink bytes —
  server or client — can receive adversarial frames. Every codec
  in this repository is expected to reject malformed input
  deterministically, without crashing, hanging, or silently
  accepting invalid content.
- **Cross-implementation disagreement.** When a Python server and
  a C++ client parse the same bytes differently, one of them is
  wrong, and that's a bug class attackers exploit. The
  differential fuzzer
  ([`security/README.md`](security/README.md)) compares all four
  codecs byte-for-byte on randomised and mutated inputs and fails
  if any two produce different results on the same wire.
- **Classic memory-safety bugs in the C and C++ cores.** Integer
  overflow, out-of-bounds reads, use-after-free, signed/unsigned
  confusion around framing lengths. Caught by libFuzzer +
  AddressSanitizer + UndefinedBehaviorSanitizer on every PR; see
  Phase 3 in [`security/README.md`](security/README.md).

The project does **not** defend against (at this stage):

- **Network-level attackers who can man-in-the-middle unencrypted
  TCP sessions.** TLS, authentication, and session policy are on
  the roadmap but not yet implemented. OpenIGTLink deployments
  should continue to rely on network segmentation until these
  land.
- **Denial-of-service via resource exhaustion above the codec
  layer.** Rate limiting and per-peer resource caps are planned
  but not yet implemented at the transport layer.
- **Supply-chain attacks on our dependencies.** The C and C++
  cores have zero runtime dependencies. The typed Python and
  TypeScript cores have a minimal, audited set (pydantic,
  crcmod, numpy optional; `ws` for WebSocket server).

---

## Memory-safety posture by language core

Different cores have different guarantees. Know which one you
rely on.

| Core | Memory-safe by construction | Hardening approach |
| --- | --- | --- |
| [`core-py/`](core-py/) | Yes (CPython) | Bounds-checked parser logic; differential fuzzer against all other cores |
| [`core-ts/`](core-ts/) | Yes (JS engines) | Strict typed parser; `DataView` with explicit offset checks; differential fuzzer |
| [`core-cpp/`](core-cpp/) | **No — C++ is not memory-safe** | `std::vector`/`std::string_view` over raw pointers wherever feasible; bounds-checked framing primitives; libFuzzer + ASan + UBSan in CI; differential fuzzer |
| [`core-c/`](core-c/) | **No — C is not memory-safe** | Minimal surface (no heap, no metadata parsing in-tree), every offset computation checked against caller-provided buffer length; libFuzzer (planned) |
| [`core-cpp/compat/`](core-cpp/compat/) | **No — mirrors upstream C++ API** | Delegates codec to `core-cpp`'s hardened runtime; pointer-returning public API exists for source compatibility with upstream consumers |

The C and C++ cores are written to be auditable — small, one
concern per translation unit, no clever metaprogramming — so
that review and fuzzing can reach meaningful coverage. That's a
deliberate trade against terseness.

---

## What the fuzzing harness guarantees (and doesn't)

As of this writing, the differential fuzzer has run:

- **1,000,000 iterations** (py-ref + py + cpp + ts) with **zero
  disagreements**.
- **200,000 iterations** including a fifth oracle that forces the
  stdlib-only (no numpy) fallback path, also zero disagreements.
- **60-second libFuzzer runs** under ASan + UBSan on every PR,
  covering the framing parser and the oracle entry point.

The fuzzer has found and we have fixed:

1. **ASCII strictness** — inconsistent non-ASCII handling across
   codecs. Unified to strict reject across all four.
2. **Length-prefixed string truncation** — Python silently
   truncated instead of rejecting on out-of-bounds lengths.
   Added bounds check.
3. **Round-trip canonicalization for float32 NaNs** — different
   language runtimes normalized signaling NaNs differently;
   defined a fixed-point canonical form and enforced it in all
   four codecs.
4. **TS NUL-padded string parsing** — early fuzzer run surfaced
   a divergence from Python/C++ behavior; fixed.
5. **C++ integer overflow in framing bounds check** (libFuzzer
   finding) — a 58-byte malformed header could drive `crc64`
   past the buffer end via `size_t` wrap. Remote DoS. Fixed;
   regression pinned in the negative corpus.

The fuzzer does **not** guarantee:

- **Completeness.** Zero disagreements on the inputs tested so
  far; not a proof of absence of bugs on all possible inputs.
- **Coverage of transport or session logic.** The fuzzer targets
  the codec surface. Transport-layer fuzzing is future work.
- **Protection against specification ambiguities.** If upstream
  OpenIGTLink is itself ambiguous about how some byte sequence
  should parse, the fuzzer can detect *that* we disagree but
  not resolve *whose interpretation is right*. We resolve those
  against upstream's test fixtures and document the decision in
  [`spec/protocol/v3.md`](spec/protocol/v3.md).

Details in [`security/README.md`](security/README.md) and
[`security/PLAN.md`](security/PLAN.md).

---

## Negative corpus

Twenty-one curated must-reject wire blobs live under
[`spec/corpus/negative/`](spec/corpus/negative/), one per known
bug class (malformed framing, length overflow, encoding
violations, etc.). Every codec's test suite parametrises over
this corpus; any codec that accepts a negative entry fails CI.

New parsing regressions should add a pinned negative entry so
the same bug can't return.

---

## Supported versions

The project is pre-1.0. All development happens on `main`; there
are no released version branches yet.

Security fixes land on `main` immediately. Once versioned
releases exist, this section will change to the conventional
"which versions receive security updates" table.

---

## Reporting a vulnerability

**Do not open a public GitHub issue for exploitable defects.**

Preferred channel: [GitHub private vulnerability
reporting](https://docs.github.com/en/code-security/security-advisories/guidance-on-reporting-and-writing-information-about-vulnerabilities/privately-reporting-a-security-vulnerability)
on this repository. That routes the report to the maintainers
without public disclosure.

Alternate channel: email the maintainer at
[anthropic@m.halle.us](mailto:anthropic@m.halle.us) with the
subject prefix `[openigtlink-ng security]`.

A good report includes:

- The affected component (codec, transport, compat shim,
  codegen, spec).
- A minimal reproducer — wire bytes, input file, or test case.
- What behavior you observed vs. what you expected.
- Whether you've verified the issue on `main` or a specific
  commit SHA.

We aim to acknowledge reports within 3 business days and to
publish a fix or mitigation plan within 30 days for exploitable
issues. Coordinated disclosure timelines can be negotiated for
issues whose fixes are structurally complex.

---

## Scope

In scope for this policy:

- All code under [`core-py/`](core-py/), [`core-ts/`](core-ts/),
  [`core-cpp/`](core-cpp/), [`core-c/`](core-c/), and
  [`corpus-tools/`](corpus-tools/).
- The compat shim under
  [`core-cpp/compat/`](core-cpp/compat/).
- The fuzzing and testing harness under
  [`security/`](security/).

Out of scope:

- The upstream `openigtlink/OpenIGTLink` library. Report to its
  maintainers.
- Third-party integrations (PLUS Toolkit, 3D Slicer, etc.) —
  unless the defect reproduces against our codecs or shim.
- The protocol specification itself as a design document —
  protocol-design discussions belong in issues or pull requests
  against [`spec/`](spec/).

---

## Credit

Contributors who responsibly disclose security issues will be
credited in the fix's commit message and the project changelog
(once one exists), unless they request otherwise.
