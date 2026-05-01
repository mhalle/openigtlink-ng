# Porting PLUS Toolkit to openigtlink-ng

A step-by-step guide for taking an existing
[PLUS Toolkit](https://github.com/PlusToolkit/PlusLib) build and
relinking it against openigtlink-ng's compat shim instead of
upstream's `libOpenIGTLink`. After the port, PLUS continues to
speak the OpenIGTLink protocol on the wire — its peers (3D Slicer,
external tracking servers, IGTLIO clients) are unaffected — but
all of PLUS's parsing and packing now goes through this project's
hardened, fuzzer-tested codec.

This guide is task-oriented. For the underlying audit and the
patch reference, see:

- [`PLUS_AUDIT.md`](PLUS_AUDIT.md) — surface audit, dependency
  landscape, what's in scope.
- [`plus-patches/README.md`](plus-patches/README.md) — the three
  unified diffs and what each one does.
- [`MIGRATION.md`](MIGRATION.md) — generic migration guide for
  any consumer of upstream's `igtl::` API; PLUS is the
  best-documented case.

---

## Why port?

The trade is straightforward:

| You keep | You get |
|---|---|
| All your PLUS code unchanged outside three custom-message files | Memory-safe parsing — bounds-checked primitives, fuzzer-discovered bugs already fixed |
| Wire compatibility with every other OpenIGTLink peer | A single static archive (`liboigtl.a`) instead of `libOpenIGTLink.a` + igtlutil |
| The exact `igtl::` API your PLUS code already uses | Active maintenance and a security disclosure path |

You do **not** get any new features at the protocol level. Bytes
on the wire are byte-identical to upstream's library — the project
verifies this against upstream's own test fixtures on every PR. If
your goal is a new message type or a new wire feature, this port
is unrelated to that.

---

## What you keep vs. what changes

PLUS sits on a stack of four upstream dependencies; we replace one
of them and leave the rest alone:

```
   ┌────────────────────────────┐
   │   PLUS Toolkit (PlusLib)   │   YOUR CODE — unchanged
   └────────────┬───────────────┘   except 3 custom-message files
                │
   ┌────────────┴───────────────┐
   │   VTK, IGSIO, IGTLIO       │   keep — link as before
   └────────────┬───────────────┘
                │
   ┌────────────┴───────────────┐
   │   OpenIGTLink C++ library  │   REPLACE with openigtlink-ng's
   │   (igtl::, igtlutil)       │   compat shim (oigtl::igtl_compat)
   └────────────────────────────┘
```

The three custom-message files
(`igtlPlusClientInfoMessage`, `igtlPlusTrackedFrameMessage`,
`igtlPlusUsMessage`) reach into protected members of
`igtl::MessageBase` — a pattern upstream silently allowed, but
which we treat as a non-public extension surface. The three
patches replace that direct access with calls to a sanctioned
tier-2 API (`CopyReceivedFrom`, `GetContentPointer`) that compiles
either way: against upstream OpenIGTLink (unchanged behaviour) or
against our shim (routes through the sanctioned methods). The
patches are small (~150 lines total across 6 files) and the
ifdef'd helpers preserve PLUS's existing pack/unpack logic.

Everything else in PLUS — its socket usage, its message factory,
its server, its client — uses only public `igtl::` API and works
without source edits. See [`PLUS_AUDIT.md`](PLUS_AUDIT.md) for the
exhaustive surface inventory.

---

## Prerequisites

You need:

1. A C++17-capable toolchain (gcc ≥ 9, clang ≥ 10, MSVC 2019+).
2. CMake ≥ 3.16.
3. A working PLUS build environment — i.e. you can already build
   PLUS against upstream `libOpenIGTLink`. If PLUS doesn't build
   for you against upstream first, debug that before swapping in
   the new library; the port is not a way around environmental
   issues.
4. A clone of openigtlink-ng (this repository).
5. A `PlusToolkit/PlusLib` working tree at whatever revision your
   downstream needs.

---

## The port, in four steps

### Step 1 — Build openigtlink-ng with the compat shim

From the openigtlink-ng repo root:

```bash
cmake -S core-cpp -B core-cpp/build \
      -DCMAKE_BUILD_TYPE=Release \
      -DOIGTL_INSTALL=ON \
      -DOIGTL_BUILD_MERGED=ON \
      -DOIGTL_DROP_IN_NAME=ON \
      -DCMAKE_INSTALL_PREFIX="$PWD/_install"
cmake --build core-cpp/build --target install -j
```

The `OIGTL_DROP_IN_NAME` flag installs the merged archive both as
`liboigtl.a` and as `libOpenIGTLink.a`, so PLUS's CMake config —
which hard-codes the upstream library name in some places — can
find us without further edits. See
[`MIGRATION.md` §"Build recipes"](MIGRATION.md#build-recipes) for
all the install-flag combinations.

After install you should have:

```
_install/
├── include/igtl/      ← compat headers (igtlMessageBase.h, etc.)
├── include/oigtl/     ← modern API headers (skip if not using it)
└── lib/
    ├── liboigtl.a            ← the merged static archive
    └── libOpenIGTLink.a      ← same archive, drop-in name
```

### Step 2 — Apply the three patches to your PLUS tree

PLUS keeps its source files as CRLF; our patches are LF. Apply
with `--ignore-whitespace` so the line-ending difference doesn't
reject every hunk:

```bash
cd PlusLib
git apply --ignore-whitespace \
    /path/to/openigtlink-ng/core-cpp/compat/plus-patches/*.patch
```

What gets touched:

- `src/PlusOpenIGTLink/igtlPlusClientInfoMessage.{h,cxx}`
- `src/PlusOpenIGTLink/igtlPlusTrackedFrameMessage.{h,cxx}`
- `src/PlusOpenIGTLink/igtlPlusUsMessage.{h,cxx}`

The patches were verified against PLUS HEAD (`PlusToolkit/PlusLib`
commit `489d0bb` as of writing), and the targeted code in those
six files has been stable across recent PLUS history. If a hunk
does reject on a divergent branch, the patches' *intent* is small
(replace direct `m_Content` / `InitBuffer` access with a
helper-method call that's ifdef-gated) — see
[`plus-patches/README.md`](plus-patches/README.md) for what each
patch does.

Each patch ifdef-gates the changed code paths on `OIGTL_NG_SHIM`:
when defined (which our shim's `igtlMacro.h` does automatically),
the helper methods route through the sanctioned `CopyReceivedFrom`
/ `GetContentPointer` API; when undefined, they replay the
original upstream sequence unchanged. So a patched PLUS tree
remains buildable against upstream OpenIGTLink — **the patches
don't burn that bridge.**

### Step 3 — Point PLUS's CMake at openigtlink-ng

PLUS's superbuild discovers OpenIGTLink via a CMake-package
mechanism. The simplest hand-off:

```bash
# In your PLUS build directory:
cmake /path/to/PlusLib \
      -DOpenIGTLink_DIR=/path/to/openigtlink-ng/_install/lib/cmake/oigtl \
      -DPLUS_USE_OpenIGTLink=ON \
      [your other PLUS flags]
```

If your PLUS build is a superbuild that fetches OpenIGTLink itself,
you'll need to redirect the superbuild to use
`/path/to/openigtlink-ng/_install` as the install prefix instead
of letting it clone upstream. PLUS's superbuild scripts vary; the
load-bearing step is that
`find_package(OpenIGTLink CONFIG REQUIRED)` resolves to our config.

### Step 4 — Build PLUS

```bash
cmake --build . -j
```

Expected outcome:

- The three patched files compile under the `OIGTL_NG_SHIM` path.
- The remaining ~5,200 LoC of PLUS OpenIGTLink code compiles
  without modification.
- Linker resolves all `igtl::` symbols against `liboigtl.a`.
- `vtkPlusOpenIGTLinkServer`, `vtkPlusOpenIGTLinkClient`, and
  every example program in PLUS link cleanly.

---

## Verification

Three checks to confirm the port worked:

### 1. Symbol resolution

```bash
nm /path/to/PlusLib/build/your-binary | grep "igtl::MessageBase"
```

You want to see references that resolve to the openigtlink-ng
archive. On Linux with `--whole-archive` builds, the symbols come
from `liboigtl.a`; check `ldd` and `nm` output to confirm no
upstream-OpenIGTLink residue remains in the link.

### 2. PLUS's own tests

PLUS ships a test suite. Run it; the OpenIGTLink-related tests
(message factory tests, server tests) should still pass. They
exercise the same code paths the port touched — the patches were
specifically designed to leave the externally visible behaviour
of `Clone()` and packed-content access unchanged.

### 3. On-wire interop

Run a PLUS server with the new library and connect a known peer
(3D Slicer, an unmodified upstream client, our own `core-py`
client). The peer must not be able to tell PLUS is using a
different library — bytes on the wire are identical.

The strong form of this check is to capture wire frames with
`tcpdump`/Wireshark and diff against frames captured from the
unported PLUS. Byte-identical sequences for the same input mean
the port preserves semantics.

---

## Troubleshooting

### "undefined reference to igtl::SomethingMessage::CloneSomething"

Your patched PLUS tree didn't define `OIGTL_NG_SHIM` during the
build. The macro is meant to be picked up automatically from our
`igtlMacro.h`, which `igtlMessageBase.h` includes. If you're
seeing this, your `-I` paths are pulling in upstream's headers
ahead of ours — re-check `OpenIGTLink_DIR` and the CMake config
PLUS is consuming.

### "undefined reference to igtl::igtl_is_little_endian"

You linked only `liboigtl.a` but PLUS's igtlutil-using code wants
the C-level helpers. Confirm `OIGTL_BUILD_MERGED=ON` was on at
install time; the merged archive includes the C runtime symbols.
If you opted for the per-component build, link `liboigtl_runtime.a`
explicitly.

### `WaitForConnection` return-type mismatch on old PLUS branches

Compile error along the lines of:

```
error: cannot convert 'igtl::ClientSocket::Pointer' to 'igtl::Socket*'
       in initialization
   ... = serverSocket->WaitForConnection(...);
```

Upstream OpenIGTLink changed `ServerSocket::WaitForConnection`'s
return type from raw `igtl::Socket*` to `ClientSocket::Pointer`
around 2020. Our shim matches the post-2020 signature, which is
also what current PLUS expects. If you are on a PLUS branch that
predates that change, cherry-pick the upstream pointer-return
update from PLUS into your tree — there's no version of the shim
that keeps the old raw-pointer return.

### Patches don't apply cleanly

The most common cause is forgetting `--ignore-whitespace` —
PLUS source files are CRLF, our patches are LF, and `git apply`
treats that as every line not matching. Re-run with the flag.

If they still reject after that, the divergence is real. Try
`git apply --3way --ignore-whitespace *.patch` to fall back to
Git's merge machinery. If a hunk continues to reject, the
*intent* of each patch is small and stable (replace direct
`m_Content` / `InitBuffer` access with a helper method,
ifdef-gate the helper body); hand-port the rejected hunk using
[`plus-patches/README.md`](plus-patches/README.md) as a
description of what each patch is meant to do.

### Wire output doesn't match upstream byte-for-byte

This shouldn't happen — wire-byte equality is a hard CI invariant
on every PR. If you observe it, please file an issue with:
- Captured frame bytes from both libraries on the same input.
- The exact message type, version, and PLUS code path that
  generated the frame.
- Your `OpenIGTLink_DIR`, library archive, and any non-standard
  build flags.

---

## Upstreaming considerations

The patches are designed to be palatable as an additive
compatibility PR to PLUS upstream:

- The ifdef shape means upstream PLUS keeps building against
  upstream OpenIGTLink with no behaviour change.
- The new helper methods (`CloneReceivedStateFrom`,
  `GetContentBytes`) are small and well-named.
- The patches actually *improve* PLUS's hygiene by replacing
  scattered protected-member access with named helpers, even on
  the upstream-OpenIGTLink path.

Whether to pursue an upstream PR is a coordination question, not
a technical one; the patches ship in this tree regardless. If you
do submit one, link to this guide and to
[`PLUS_AUDIT.md`](PLUS_AUDIT.md) for the rationale.

---

## What this guide doesn't cover

- **Replacing VTK, IGSIO, or IGTLIO.** None of those are this
  project's responsibility. PLUS continues to use them unchanged.
- **PLUS's non-OpenIGTLink protocol surfaces.** PLUS speaks
  several protocols (OpenIGTLink, USB-attached devices, custom
  serial). Only the OpenIGTLink path is affected by this port.
- **Building PLUS itself for the first time.** PLUS has its own
  documentation; if you can't build PLUS against upstream
  OpenIGTLink, debug that first. This guide assumes you have a
  working baseline.
- **Compiling PLUS sources directly into our test suite.** That
  hypothetical `compat_plus_interop` build would require stubs
  for VTK + IGSIO + IGTLIO and is a separate workstream, not
  covered here.
