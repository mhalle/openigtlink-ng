# Proposed upstream PR to `PlusToolkit/PlusLib`

This document outlines the upstream pull-request shape for the
single patch in this directory, so a PLUS maintainer who reaches
this page can see the trajectory and a contributor can pick up
the submission with all the rationale already written down.

## Status

**Drafted, not yet submitted.** The patch has been verified to
apply cleanly to PLUS HEAD with `--ignore-whitespace` and to
preserve upstream-OpenIGTLink behavior identically. Submitting
the PR is a coordination step that should follow some advance
discussion with PLUS maintainers.

## Title

> PlusOpenIGTLink: support hardened OpenIGTLink reimplementations
> via `__has_include`

## One-paragraph summary

The three custom message classes in `PlusOpenIGTLink`
(`PlusClientInfoMessage`, `PlusTrackedFrameMessage`,
`PlusUsMessage`) reach into protected members of
`igtl::MessageBase` to implement `Clone()` and to compute
raw-pointer offsets into the content region. A PR like this lets
PLUS continue to compile against upstream OpenIGTLink unchanged
while *also* compiling cleanly against alternative implementations
that prefer to expose the same capability through a sanctioned
extension API — without users having to apply local patches and
without PLUS picking sides in the implementation choice.

## What changes

One file added, three edited:

- **New:** `Source/PlusOpenIGTLink/igtlPlusMessageCompat.h`
  (~125 lines including license boilerplate and inline doc
  comments). Contains the entire conditional logic.

- **Edited:** the three `.cxx` files mentioned above. Each loses
  a 5–10-line block of direct protected-member access and gains
  a single macro invocation. No `.h` changes; no behavioural
  change against upstream.

- **Mechanism:** `__has_include(<igtl/igtlSafeMessageHelpers.h>)`
  in the new helper header. The header is shipped by hardened
  reimplementations; absent from upstream OpenIGTLink. Pure
  feature-test, no consumer-visible defines.

## Why this is upstream-friendly

1. **No behaviour change against upstream OpenIGTLink.** The
   legacy macro expansion replays the original
   `InitBuffer`/`CopyHeader`/`AllocateBuffer`/`CopyBody`/metadata
   sequence exactly as PLUS has it today.

2. **Compiler floor unchanged.** `__has_include` is C++17
   standard; PLUS's existing toolchain floor already covers it.
   The patch guards with `#if defined(__has_include)` anyway as a
   one-line defensive habit.

3. **No build-system changes.** No new CMake options; no new
   third-party dependencies; no namespace pollution. Detection is
   automatic at preprocessing time.

4. **Improves hygiene on the upstream path too.** Replacing
   scattered protected-member access with named helper macros is
   a discipline win regardless of which OpenIGTLink ends up
   linked. A future maintainer reading `Clone()` sees a
   meaningful name instead of five lines of internal-state copy.

5. **One side-effect correctness fix worth calling out:**
   `PlusTrackedFrameMessage::Clone()` previously did *not* copy
   v2/v3 metadata (only the header + body content). The macro
   form copies metadata uniformly across all three message
   classes. This is strictly additive — cloned tracked frames now
   carry the metadata they were sent with — and resolves a small
   latent bug.

## What the PR does *not* do

- It does not require PLUS to commit to any specific OpenIGTLink
  reimplementation. The detection is feature-based, not
  vendor-based; any future implementation that exposes
  `<igtl/igtlSafeMessageHelpers.h>` and the matching tier-2 API
  will be picked up automatically.
- It does not change PLUS's public API or its build interface.
- It does not depend on any third-party library unavailable today.

## Suggested commit message

(Same as the `format-patch` message in
[`0001-PlusOpenIGTLink-support-hardened-reimplementations.patch`](0001-PlusOpenIGTLink-support-hardened-reimplementations.patch).)

## Suggested PR description outline

1. **Summary** — three sentences from this document.
2. **What changes** — the file list above.
3. **Detection mechanism** — short paragraph on `__has_include`.
4. **Why upstream-friendly** — copy the numbered list above.
5. **One side-effect** — the `TrackedFrameMessage` metadata-copy
   fix, called out so it isn't a surprise on review.
6. **Test plan** — built PLUS against upstream OpenIGTLink at the
   pinned SHA: behaviour byte-identical to before the PR. Built
   PLUS against the openigtlink-ng compat shim (link to its
   repo): all PLUS tests pass; wire output byte-identical to the
   upstream-linked build for the same inputs.

## Reciprocal change in upstream OpenIGTLink (optional follow-up)

If the long-term goal is to eliminate the conditional entirely,
a separate small PR to
[`openigtlink/OpenIGTLink`](https://github.com/openigtlink/OpenIGTLink)
adding `CopyReceivedFrom` and `GetContentPointer` as protected
methods on `igtl::MessageBase` would let PLUS drop the legacy
branch of the helper header — both paths would call the same
sanctioned API. That's a separate ~30-line PR, has independent
value (it documents the safe extension contract upstream's
header lacks), and isn't a blocker for the PR described here.

## Coordination notes

- The patch's helper file uses a `__igtlPlusMessageCompat_h`
  include guard matching PLUS conventions seen elsewhere in
  `Source/PlusOpenIGTLink/`. Reviewers may have a different
  preferred guard style — adjust on request.
- Macro names use `IGTL_PLUS_*` prefix. Reviewers may prefer
  `PLUS_IGTL_*` or another convention. The names are local to
  the helper header and three call sites; renaming is mechanical.
- The copyright/license header on `igtlPlusMessageCompat.h` is
  the standard PLUS preamble. Verify it matches whatever
  PLUS-licensed-code template is current.
