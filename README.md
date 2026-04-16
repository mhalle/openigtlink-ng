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

- [`spec/`](spec/) — protocol specification, machine-readable message
  schemas, and conformance corpus. The **specification of record**.
- [`core-cpp/`](core-cpp/) — reference C++17 implementation and the
  legacy API compatibility shim.
- [`corpus-tools/`](corpus-tools/) — corpus generator, validator,
  differential-fuzz harness.

## Status

Early design / scaffolding phase. No shipping artifacts yet. See
`spec/` for specification drafts and `../openigtlink/` for the current
hardening audit work that this project inherits and extends.

## License

Apache License 2.0. See [LICENSE](LICENSE).
