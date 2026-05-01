# PLUS Toolkit → shim API-surface audit

> **Looking to actually port PLUS?** Start with
> [`PORTING_PLUS.md`](PORTING_PLUS.md) — the task-oriented,
> step-by-step guide. This document is the reference behind it:
> the surface audit, dependency landscape, and the rationale for
> what's in scope.


**Pinned PLUS SHA:** `474c3f266abc2417f45b14085b2c6b793efea311`
(PlusToolkit/PlusLib, fetched via sparse checkout of `src/PlusOpenIGTLink/`
and `src/PlusServer/`.)

**Goal:** quantify how much of PLUS's OpenIGTLink API surface our compat
shim already covers, and identify every gap that would block a drop-in
replacement for upstream's `openigtlink/OpenIGTLink` C++ library.

## Source files audited

14 files across two PLUS modules, ~5.2 KLoC total:

| File | LoC |
| --- | ---: |
| `src/PlusOpenIGTLink/PlusIgtlClientInfo.{h,cxx}` | 435 |
| `src/PlusOpenIGTLink/igtlPlusClientInfoMessage.{h,cxx}` | 75 |
| `src/PlusOpenIGTLink/igtlPlusTrackedFrameMessage.{h,cxx}` | 220 |
| `src/PlusOpenIGTLink/igtlPlusUsMessage.{h,cxx}` | 286 |
| `src/PlusOpenIGTLink/vtkPlusIGTLMessageQueue.{h,cxx}` | 73 |
| `src/PlusOpenIGTLink/vtkPlusIgtlMessageCommon.{h,cxx}` | 999 |
| `src/PlusOpenIGTLink/vtkPlusIgtlMessageFactory.{h,cxx}` | 528 |
| `src/PlusServer/vtkPlusOpenIGTLinkClient.{h,cxx}` | 427 |
| `src/PlusServer/vtkPlusOpenIGTLinkServer.{h,cxx}` | 1532 |
| platform-specific server main shells | 155 |

## `igtl::` symbols referenced by PLUS

All of the following are **present** in the shim as of this commit
(verified by `compat/tests/plus_header_surface_test.cxx`, which
pulls every matching header and declares one Pointer of each type):

| Category | Symbols |
| --- | --- |
| **Messaging base** | `MessageBase`, `MessageHeader`, `MessageFactory`, `TimeStamp` |
| **Messages — always-on** | `TransformMessage`, `PositionMessage`, `StatusMessage`, `GetStatusMessage`, `ImageMessage`, `GetImageMessage`, `StringMessage`, `CommandMessage`, `RTSCommandMessage` |
| **Messages — v2/v3** | `PointMessage`, `PointElement`, `GetPointMessage`, `PolyDataMessage`, `GetPolyDataMessage`, `RTSPolyDataMessage`, `TrackingDataMessage`, `TrackingDataElement`, `StartTrackingDataMessage`, `StopTrackingDataMessage`, `RTSTrackingDataMessage`, `ImageMetaMessage`, `ImageMetaElement`, `GetImageMetaMessage` |
| **Transport** | `ClientSocket`, `ServerSocket`, `Socket` |
| **Math helpers** | `Matrix4x4`, `IdentityMatrix`, `MatrixToQuaternion`, `QuaternionToMatrix` |
| **Protocol helpers** | `IGTLProtocolToHeaderLookup` (added this commit) |

Symbols called out during audit and **confirmed present** (not missing
despite being named in the plan):
- `igtl::RTSCommandMessage` — lives in `igtlCommandMessage.h`, matching
  upstream's layout (upstream has no separate `igtlRTSCommandMessage.h`).
- `igtl::RTSPolyDataMessage` — lives in `igtlPolyDataMessage.h`.
- `igtl::PointElement` — lives in `igtlPointMessage.h`.
- `igtl::MessageFactory` — added this commit as a one-for-one port of
  upstream's `Source/igtlMessageFactory.{h,cxx}`.

## `IGTL_*` macros and C-level helpers

All of the following are now provided by the shim's C-level headers
(`compat/include/igtl/igtl_*.h`):

| Macro / function | Upstream origin | Shim home |
| --- | --- | --- |
| `IGTL_HEADER_VERSION_1/_2`, `IGTL_HEADER_SIZE` | `igtlutil/igtl_header.h` | `igtl_header.h` |
| `OpenIGTLink_PROTOCOL_VERSION_1/_2/_3` | `igtlConfigure.h` | `igtl_header.h` |
| `IGTL_IMAGE_HEADER_SIZE` | `igtlutil/igtl_image.h` | `igtl_image.h` |
| `IGTL_TDATA_LEN_NAME` | `igtlutil/igtl_tdata.h` | `igtl_tdata.h` |
| `IGTL_VIDEO_ENDIAN_BIG/_LITTLE` | `VideoStreaming/igtl_video.h` | `igtl_video.h` |
| `igtl_is_little_endian()` | `igtlutil/igtl_util.h` | `igtl_util.h` (inline) |
| `igtl_types.h`, `igtl_win32header.h` forwarders | — | alias to camelCase shim header |

## Out of scope for the shim

The PLUS source tree depends on three non-OIGTL upstream-of-us
libraries that are *not* our responsibility to stub:

1. **VTK core** — `vtkObject`, `vtkMatrix4x4`, `vtkImageData`,
   `vtkPolyData`, `vtkMultiThreader`, `vtkXMLUtilities`, and everything
   under `vtksys/`. PLUS links VTK regardless of which IGTL it uses.
2. **IGSIO** — `igsioTrackedFrame`, `igsioVideoFrame`,
   `vtkIGSIOTrackedFrameList`, `vtkIGSIORecursiveCriticalSection`,
   `vtkIGSIOTransformRepository`, `vtkIGSIOFrameConverter`.
3. **igtlio** (separate Slicer-adjacent library) —
   `igtlioConverterUtilities`, `igtlioImageConverter`,
   `igtlioPolyDataConverter`, `igtlioTransformConverter`. Only
   referenced in `vtkPlusIgtlMessageCommon.cxx` for legacy converter
   utilities.

A full `compat_plus_interop` build that compiles all 14 PLUS sources
would need stubs for each. Realistic cost: several kloc of shim (not
the 200-LoC budget the original plan assumed), so it's deferred
pending a concrete consumer that needs it.

## What this commit proves

`compat_plus_header_surface` (CTest target
`igtl_compat_plus_header_surface_test`) pulls in the complete set of
`igtl*.h` headers PLUS includes and declares one instance of every
referenced class, plus references every called helper function and
macro. A green build demonstrates:

- every PLUS-referenced `igtl::` symbol resolves against the shim;
- every `IGTL_*` macro PLUS tests against is defined with the
  upstream value;
- the C-level helpers PLUS calls (`igtl_is_little_endian`) link.

That's the portion of "drop-in replacement for upstream" we can
honestly claim without also shimming VTK + IGSIO + igtlio. A
future Phase 3b that takes on those upstream-of-us dependencies
can drop PLUS sources straight into `compat_plus_interop`.
