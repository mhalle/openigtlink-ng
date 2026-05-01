# core-cpp / compat — the upstream `igtl::` shim

This directory re-exposes the upstream
[OpenIGTLink](https://github.com/openigtlink/OpenIGTLink) C++ API
(`igtl::ClientSocket`, `igtl::TransformMessage`, `igtl::MessageBase`,
…) on top of the openigtlink-ng codec. **Existing C++ code written
against `libOpenIGTLink` recompiles and links against this without
source changes.**

## What this is, in one paragraph

Most existing OpenIGTLink users equate "OpenIGTLink" with upstream's
C++ library — when their code says `#include "igtlTransformMessage.h"`
and `igtl::ClientSocket::New()`, that's the *library API*, distinct
from the *wire protocol*. openigtlink-ng separates the two:

```
                   ┌─────────────────────────────────┐
                   │   YOUR APPLICATION              │
                   │   #include "igtlTransform...h"  │
                   │   igtl::ClientSocket::New()     │
                   └────────────┬────────────────────┘
                                │
                ┌───────────────┴────────────────┐
                │   compat/  (this directory)    │
                │   re-exposes the igtl:: API    │
                └───────────────┬────────────────┘
                                │ delegates to
                ┌───────────────┴────────────────┐
                │   core-cpp/  (modern oigtl::)  │
                │   bounds-checked codec +       │
                │   fuzzer-hardened parsing      │
                └────────────────────────────────┘
                                │ produces
                                ▼
              ─── identical bytes on the wire ───
```

You're switching libraries, not protocols. Every byte the shim
emits is identical to what upstream's library would emit; CI
verifies this against upstream's own test fixtures on every PR.

## When to use the shim vs. the modern API

| You are… | Use |
|---|---|
| Maintaining an existing C++ application written against `libOpenIGTLink` | **The compat shim** — recompile, no source edits |
| Writing new C++ code from scratch | **The modern `oigtl::` API** — see [`../README.md`](../README.md) and [`../CLIENT_GUIDE.md`](../CLIENT_GUIDE.md) |
| Integrating a third-party library that uses `igtl::` (PLUS, Slicer, IGTLIO) | **The compat shim** — link your dependency against `liboigtl.a`, the headers it expects are there |

The two APIs share the same underlying codec — you can mix them
in the same translation unit if you need to. See
[`MIGRATION.md`](MIGRATION.md) §"Mixing compat and modern API".

## Sub-documents

- **[`MIGRATION.md`](MIGRATION.md)** — full migration guide. Linker
  flags, build recipes, behavioral differences from upstream,
  troubleshooting, FAQ. **Start here if you are an existing
  upstream user.**
- **[`PORTING_PLUS.md`](PORTING_PLUS.md)** — task-oriented,
  step-by-step guide for porting an existing PLUS Toolkit build
  to use this shim. End-to-end, with verification and
  troubleshooting. **Start here if you are a PLUS user.**
- **[`API_COVERAGE.md`](API_COVERAGE.md)** — exhaustive
  per-class/per-method coverage matrix. Documents which upstream
  symbols the shim provides, which it stubs, and which it
  deliberately omits.
- **[`PLUS_AUDIT.md`](PLUS_AUDIT.md)** — focused audit of PLUS's
  interaction with the shim. The reference behind PORTING_PLUS:
  surface inventory, dependency landscape, scope decisions.
- **[`plus-patches/`](plus-patches/)** — the unified diffs
  themselves, against a pinned PLUS commit.

## Why a source-level shim, not a binary `.so` drop-in?

A binary-level drop-in would force us to mirror upstream's ABI
*and* its internals (including bugs). A source-level shim recompiles
your code against headers we control, so the bytes the shim sends
go through our hardened parsing path. Same API to your code; safer
internals beneath it. The trade-off is that you recompile; we
believe this is the right side of the trade.

## What this is *not*

- **Not a fork of upstream.** Upstream's C++ library remains the
  reference v2/v3 implementation; this project complements it. The
  shim re-exposes *the API surface*, not the source.
- **Not a v2/v3 protocol change.** The wire format is unchanged.
  See the parent [`README.md`](../../README.md) for the full
  project framing.
- **Not where the codec lives.** All packing, parsing, CRC, and
  framing happens in [`../src/runtime/`](../src/runtime/) and
  [`../src/messages/`](../src/messages/). This directory is a
  thin translator from `igtl::` calls to those.
