# PLUS patch for openigtlink-ng compatibility

A single patch against PLUS Toolkit that lets the same source
compile cleanly against either upstream OpenIGTLink **or** the
openigtlink-ng compat shim, with no per-build configuration.

> **End-to-end porting guide:**
> [`../PORTING_PLUS.md`](../PORTING_PLUS.md) walks through the
> full PLUS port (build openigtlink-ng → check out PLUS → apply
> this patch → repoint CMake → build → verify). This README is
> the patch-level reference.

The motivation and full design discussion are in
[`../API_COVERAGE.md`](../API_COVERAGE.md) §"Subclass extension
API (tier 2)"; this README covers the patch itself.

## What the patch does

Adds **one new file** to PLUS — `igtlPlusMessageCompat.h` — that
self-detects which OpenIGTLink build is on the include path via
`__has_include(<igtl/igtlSafeMessageHelpers.h>)` and provides
two macros:

| Macro | Replaces |
|---|---|
| `IGTL_PLUS_CLONE_RECEIVED_STATE(self, src)` | The historical 5–8-line `InitBuffer` / `CopyHeader` / `AllocateBuffer` / `CopyBody` / metadata-copy sequence inside each `Clone()`. |
| `IGTL_PLUS_CONTENT_BYTES(self)` | Direct `this->m_Content` casts in pack/unpack code. |

Both macros expand inside a member function of a derived class
(so the protected-member access is legal). The conditional
choice between the safe path (sanctioned tier-2 API on our shim)
and the upstream path (raw protected-member sequence) lives
entirely in the helper header — no `#ifdef`s in the three
modified `.cxx` files.

The patch then modifies three existing PLUS source files to use
the macros:

- `src/PlusOpenIGTLink/igtlPlusClientInfoMessage.cxx`
- `src/PlusOpenIGTLink/igtlPlusTrackedFrameMessage.cxx`
- `src/PlusOpenIGTLink/igtlPlusUsMessage.cxx`

Each `.cxx` change is small (~8 lines added or modified per
file). The corresponding `.h` files are not touched.

## Pinned PLUS revision

Verified against `PlusToolkit/PlusLib@489d0bb` (HEAD as of
authoring). The patch's `.cxx` targets are stable across recent
PLUS history; the patch usually applies cleanly on nearby
revisions with `--ignore-whitespace`.

## Applying

From a PLUS working tree (current HEAD or the pinned SHA both
work):

```bash
cd PlusLib
git apply --ignore-whitespace \
    /path/to/openigtlink-ng/core-cpp/compat/plus-patches/*.patch
```

`--ignore-whitespace` is required because PLUS keeps its source
files as CRLF (Windows line endings) while these patches are LF.
Without it, every hunk rejects on `\r` mismatches even though
the substance matches.

## Compile-time behavior

| PLUS linked against | Path taken | Mechanism |
|---|---|---|
| upstream OpenIGTLink | original protected-member sequence (unchanged behaviour) | `<igtl/igtlSafeMessageHelpers.h>` not on include path → `__has_include` returns 0 → macros expand to the legacy block |
| openigtlink-ng compat shim | sanctioned tier-2 API via `MessageBase::CopyReceivedFrom` / `GetContentPointer` | shim ships `<igtl/igtlSafeMessageHelpers.h>` → `__has_include` returns 1 → macros expand to single-call sanctioned API |

In both cases, the `.cxx` source PLUS maintains is identical.
The `igtlPlusMessageCompat.h` helper file is the only place
holding conditional logic, and a reader can read it top-to-bottom
to understand both paths.

## Side-effect: `PlusTrackedFrameMessage::Clone()` now copies metadata

Prior to this patch, `PlusTrackedFrameMessage::Clone()` did not
copy v2/v3 metadata (only the header + body content). The macro
form is uniform across the three message classes and includes
the metadata copy in the legacy path. This is a strictly-additive
correctness improvement: a cloned `TRACKEDFRAME` now carries the
same metadata as the original. Worth flagging in the upstream
PR description.

## Patch shape

| Patch | Lines added | Lines removed | Files |
|---|---:|---:|---|
| `0001-PlusOpenIGTLink-support-hardened-reimplementations.patch` | ~136 | ~42 | 4 (one new + three edited) |

The historical three-patch split (one per modified class) was
collapsed into a single patch when the per-class methods were
replaced with shared macros — there's nothing left to split
sensibly.

## Compiler floor

`__has_include` is C++17 standard; PLUS's existing toolchain
floor (C++17, modern VTK + IGSIO) covers it on every supported
compiler (GCC ≥ 5, Clang / Apple Clang, MSVC ≥ 2017 15.3).

The patch defends against pre-C++17 toolchains anyway by guarding
the test:

```cpp
#if defined(__has_include)
#  if __has_include(<igtl/igtlSafeMessageHelpers.h>)
     // ...
#  endif
#endif
```

Costs one line and makes the patch reviewable as "obviously
safe" on any toolchain a PLUS reviewer might encounter.

## Not in this patch

- **Phase 3b `compat_plus_interop` compile test.** Dropping PLUS
  sources into our CI build is a separate workstream (requires
  VTK / IGSIO / igtlio stubs — see
  [`../PLUS_AUDIT.md`](../PLUS_AUDIT.md)). This patch lets that
  workstream proceed when somebody chooses to take it on.
- **Upstream PR to PlusToolkit/PlusLib.** The patch is designed
  to be palatable as an additive compatibility PR. Whether and
  when to send it is a coordination question; the patch ships in
  this tree regardless. See
  [`../PORTING_PLUS.md`](../PORTING_PLUS.md) §"Upstreaming
  considerations" and [`UPSTREAM_PR.md`](UPSTREAM_PR.md) for the
  rationale and submission outline.
- **Patches for PLUS code outside the three custom-message
  files.** The audit confirmed everything else in PLUS uses the
  public API only — no further patches needed.
