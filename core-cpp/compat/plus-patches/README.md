# PLUS patches for openigtlink-ng compatibility

Three small patches against PLUS Toolkit's three custom-message
classes that let unmodified PLUS link against our shim when
`OIGTL_NG_SHIM` is defined, while remaining byte-identical against
upstream OpenIGTLink when it isn't.

> **End-to-end porting guide:**
> [`../PORTING_PLUS.md`](../PORTING_PLUS.md) walks through the
> full PLUS port (build openigtlink-ng → check out PLUS → apply
> these patches → repoint CMake → build → verify). This README
> is the patch-level reference.

The motivation and full design discussion live in
[`../API_COVERAGE.md`](../API_COVERAGE.md) §"Subclass extension
API (tier 2)"; this README covers how to apply them.

## Pinned PLUS SHA

The patches are built against
**`PlusToolkit/PlusLib@474c3f266abc2417f45b14085b2c6b793efea311`**,
the same SHA pinned for our surface audit. They may apply cleanly
against nearby revisions but are only verified against this SHA.
Update when we re-pin PLUS.

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
Without it, every hunk rejects on `\r` mismatches even though the
substance matches.

## What each patch does

**`0001-PlusClientInfoMessage-gate-clone-on-OIGTL_NG_SHIM.patch`**
Replaces the hand-written `Clone()` body — five lines of direct
access into `MessageBase`'s protected internals — with a single
call to a new `CloneReceivedStateFrom()` helper. The helper's
body is `#ifdef`-gated: `OIGTL_NG_SHIM` defined routes to
`MessageBase::CopyReceivedFrom`, the sanctioned tier-2 extension
API on the hardened shim; undefined replays the original upstream
sequence (`InitBuffer` / `CopyHeader` / `AllocateBuffer` /
`CopyBody` / metadata-state copy).

**`0002-PlusTrackedFrameMessage-gate-clone-and-content-access.patch`**
Same `Clone()` treatment plus a second helper
(`GetContentBytes()`) replaces four call sites that cast
`this->m_Content + offset` as a raw pointer. Against the shim
the helper delegates to `GetContentPointer()`; against upstream
it returns the raw member unchanged. PLUS's pack/unpack code is
otherwise untouched.

**`0003-PlusUsMessage-gate-clone-on-OIGTL_NG_SHIM.patch`**
Same `Clone()` treatment. `PlusUsMessage` does not cast
`m_Content` as a pointer elsewhere, so no content-access
helper is needed.

## Patch sizes

| Patch | Additions | Deletions |
| --- | ---: | ---: |
| 0001 | ~30 lines | ~17 lines |
| 0002 | ~45 lines | ~20 lines |
| 0003 | ~30 lines | ~17 lines |
| **Total** | **~105 lines added, ~54 lines removed** across 6 files |

## Compile-time behavior

| Build target | `OIGTL_NG_SHIM` | Path taken |
| --- | :---: | --- |
| PLUS against upstream OpenIGTLink | undefined | Original upstream idiom (unchanged behaviour) |
| PLUS against openigtlink-ng shim | defined by `igtlMacro.h` | Sanctioned tier-2 API via `CopyReceivedFrom` / `GetContentPointer` |

In both cases, the `Clone()` call sites in PLUS's code look
identical — the ifdef surface is confined to the helper-method
bodies.

## Not in this patch set

- **Phase 3b `compat_plus_interop` compile test.** Dropping PLUS
  sources into our CI build is a separate workstream (requires
  VTK / IGSIO / igtlio stubs — see
  [`../PLUS_AUDIT.md`](../PLUS_AUDIT.md)). These patches let that
  workstream proceed when somebody chooses to take it on.
- **Upstream PR to PlusToolkit/PlusLib.** The ifdef shape is
  designed to be palatable as an additive compatibility PR. When
  and whether to pursue that is a social/coordination question,
  not a technical one; the patches ship in this tree regardless.
- **Patches for PLUS code outside the three custom-message
  files.** The audit confirmed everything else in PLUS uses the
  public API only — no further patches needed.
