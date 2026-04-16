# core-cpp

Reference C++17 implementation of the OpenIGTLink protocol and the
legacy API compatibility shim.

## Status

Scaffold. Nothing is implemented yet.

## Planned layout

```
core-cpp/
├── include/igtl_ng/   public headers (C++17-clean)
├── src/               implementation (may use C++20 internally)
│   ├── framing/       v1/v2/v3 framers, trait-based
│   ├── transport/     ASIO-based TCP/UDP/WebSocket + TLS decorator
│   ├── messages/      codegen output + hand-written extras
│   ├── session/       handshake, policy enforcement
│   └── reader_writer/ bounds-checked cursor primitives
├── schema/            schema consumer / codegen tool
├── compat/            optional shim exposing legacy igtl:: API
├── tests/
│   ├── unit/
│   ├── corpus/        runs the spec corpus from ../spec/corpus/
│   └── fuzz/          libFuzzer harnesses
├── benches/           performance regression suite
├── examples/
└── cmake/
```

## Compiler floor

Matches Kitware (VTK 9.5 / ITK 5.4) and Google's foundational C++
matrix:

- GCC ≥ 7.5
- Clang ≥ 14
- MSVC ≥ Visual Studio 2022
- Apple Clang ≥ 17

Public API is C++17-only. Internal code may use C++20 features
(coroutines, concepts, `std::span`, `std::jthread`) guarded behind
feature-test macros where they would otherwise leak into the public
surface.

## Dependencies

Intended:

- Standalone ASIO (not Boost) — async transport layer
- OpenSSL — TLS for any transport
- `tl::expected` (vendored) — `Result`-style error handling
- `gsl::span` or vendored equivalent — non-owning span type for C++17
- `fmt` — formatting (until `std::format` is universal)

No VTK, no ITK, no rendering, no file I/O for imaging formats.

## License

Apache 2.0. See [`../LICENSE`](../LICENSE).
