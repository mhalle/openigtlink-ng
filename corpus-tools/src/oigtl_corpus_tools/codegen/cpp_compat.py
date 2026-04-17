"""C++ upstream-compat shim codegen.

Emits `igtl::FooMessage` facades under ``core-cpp/compat/include/igtl/``
plus stubbed ``.cxx`` files under ``core-cpp/compat/src/``. Each
facade inherits from :class:`igtl::MessageBase` (or
:class:`HeaderOnlyMessageBase` for header-only variants) and carries
the correct ``m_SendMessageType`` string so wire output identifies
itself exactly as upstream does.

**Scope of this generator (Phase 6b-i/ii):**

- Header-only variants (GET_*, STT_*, STP_*, RTS_*, plus STP_IMAGE /
  STP_POLYDATA / STP_TDATA which are upstream-shaped header-only) —
  fully functional: they can be packed/unpacked via the inherited
  MessageBase path with `body_size == 0`, and match upstream byte
  for byte.
- Data-carrying message types get a stub facade: correct class name,
  correct `m_SendMessageType`, no typed accessors, no PackContent /
  UnpackContent override. `Pack()` on such a stub returns a
  content-less wire message (header + empty body). Consumers calling
  stub Set* methods won't compile; that's intentional — a stub is a
  compile-time contract that says "this class exists, its accessors
  need to be added to core-cpp/compat/src/".

The mapping from type_id → upstream class name is deliberately
tabular and explicit — upstream's names don't follow a mechanical
rule (STP_BIND → StopBindMessage, RTS_BIND → RTSBindMessage,
GET_TRANS → GetTransformMessage, etc.). Scraped once from upstream
headers and vendored here.
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path


# ---------------------------------------------------------------------------
# The mapping: type_id → (upstream_class_name, header_only)
# ---------------------------------------------------------------------------

# Exact-match set scraped from upstream's headers + .cxx. Extending
# beyond this would be harmless (extra classes don't break consumers)
# but would also be fictitious — we're a compat shim, not a
# speculative expansion. Stick with what upstream ships.
#
# Data-carrying (full-facade-needed) messages.
_DATA_MESSAGES: dict[str, str] = {
    "BIND":       "BindMessage",
    "CAPABILITY": "CapabilityMessage",
    "COLORT":     "ColorTableMessage",
    "COMMAND":    "CommandMessage",
    "IMAGE":      "ImageMessage",
    "IMGMETA":    "ImageMetaMessage",
    "LBMETA":     "LabelMetaMessage",
    "NDARRAY":    "NDArrayMessage",
    "POINT":      "PointMessage",
    "POLYDATA":   "PolyDataMessage",
    # POSITION is hand-written; intentionally omitted.
    "QTDATA":     "QuaternionTrackingDataMessage",
    "QUERY":      "QueryMessage",
    "SENSOR":     "SensorMessage",
    # STATUS is hand-written; intentionally omitted.
    # STRING is hand-written; intentionally omitted.
    "TDATA":      "TrackingDataMessage",
    "TRAJ":       "TrajectoryMessage",
    # TRANSFORM is hand-written; intentionally omitted.
}

# Header-only variants upstream actually ships.
_HEADER_ONLY: dict[str, str] = {
    "GET_BIND":     "GetBindMessage",
    "GET_COLORT":   "GetColorTableMessage",
    "GET_IMAGE":    "GetImageMessage",
    "GET_IMGMETA":  "GetImageMetaMessage",
    "GET_LBMETA":   "GetLabelMetaMessage",
    "GET_POINT":    "GetPointMessage",
    "GET_POLYDATA": "GetPolyDataMessage",
    "GET_STATUS":   "GetStatusMessage",
    "GET_TRAJ":     "GetTrajectoryMessage",
    # GET_TRANS skipped — hand-written alongside TransformMessage.

    "STT_BIND":     "StartBindMessage",
    "STT_POLYDATA": "StartPolyDataMessage",
    "STT_QTDATA":   "StartQuaternionTrackingDataMessage",
    "STT_TDATA":    "StartTrackingDataMessage",

    "STP_BIND":     "StopBindMessage",
    "STP_IMAGE":    "StopImageMessage",
    "STP_POLYDATA": "StopPolyDataMessage",
    "STP_QTDATA":   "StopQuaternionTrackingDataMessage",
    "STP_TDATA":    "StopTrackingDataMessage",

    "RTS_BIND":     "RTSBindMessage",
    "RTS_POLYDATA": "RTSPolyDataMessage",
    "RTS_QTDATA":   "RTSQuaternionTrackingDataMessage",
    "RTS_TDATA":    "RTSTrackingDataMessage",
    # RTS_COMMAND: upstream ships the class but we default-implement
    # it via a data stub alongside CommandMessage; consumers can
    # still New() it.
    "RTS_COMMAND":  "RTSCommandMessage",
}


# ---------------------------------------------------------------------------
# Header / source text generation
# ---------------------------------------------------------------------------


@dataclass
class ShimFile:
    """One generated header (+ optional .cxx) for a facade class."""
    class_name: str
    header_path: Path          # relative to compat/include/igtl/
    cxx_path: Path | None      # relative to compat/src/
    hpp_text: str
    cxx_text: str | None


def _header_only_hpp(type_id: str, class_name: str) -> str:
    """Emit a fully-functional header-only facade.

    Inherits from `MessageBase` (not `HeaderOnlyMessageBase`) because
    upstream itself inconsistently parents these — some against
    HeaderOnlyMessageBase, some against MessageBase. MessageBase is
    the strict superset: bodyless messages pack fine through its
    pipeline with `CalculateContentBufferSize() == 0`.
    """
    guard = f"__igtl{class_name}_h"
    return f"""\
// GENERATED by corpus-tools/oigtl-corpus codegen cpp-compat — do not edit.
//
// Shim facade for upstream's `igtl::{class_name}` (type_id: {type_id}).
// Header-only (no body content).

#ifndef {guard}
#define {guard}

#include "igtlMacro.h"
#include "igtlMessageBase.h"

namespace igtl {{

class IGTLCommon_EXPORT {class_name} : public MessageBase {{
 public:
    igtlTypeMacro(igtl::{class_name}, igtl::MessageBase);
    igtlNewMacro(igtl::{class_name});

 protected:
    {class_name}()  {{ m_SendMessageType = "{type_id}"; }}
    ~{class_name}() override = default;
}};

}}  // namespace igtl

#endif  // {guard}
"""


def _data_stub_hpp(type_id: str, class_name: str) -> str:
    """Emit a stub facade for a data-carrying message.

    The class exists, carries the right type_id, and compiles.
    Pack() returns a header-only wire message (empty body). A
    hand-written .cxx later replaces this stub with full accessors
    and PackContent/UnpackContent overrides.
    """
    guard = f"__igtl{class_name}_h"
    return f"""\
// GENERATED by corpus-tools/oigtl-corpus codegen cpp-compat — do not edit
// header. Pack/unpack behaviour is STUBBED: this class exists for
// API completeness but carries no typed accessors. To implement,
// replace with a hand-written pair in core-cpp/compat/include/igtl/
// and core-cpp/compat/src/ (see igtlTransformMessage.{{h,cxx}} for
// the pattern).
//
// Shim facade for upstream's `igtl::{class_name}` (type_id: {type_id}).

#ifndef __igtl{class_name}_h
#define __igtl{class_name}_h

#include "igtlMacro.h"
#include "igtlMessageBase.h"

namespace igtl {{

class IGTLCommon_EXPORT {class_name} : public MessageBase {{
 public:
    igtlTypeMacro(igtl::{class_name}, igtl::MessageBase);
    igtlNewMacro(igtl::{class_name});

    // No typed accessors — hand-written replacement required for
    // byte-exact parity with upstream.

 protected:
    {class_name}()  {{ m_SendMessageType = "{type_id}"; }}
    ~{class_name}() override = default;
}};

}}  // namespace igtl

#endif  // {guard}
"""


# ---------------------------------------------------------------------------
# Entry points
# ---------------------------------------------------------------------------


def all_shim_files(
    *,
    skip_type_ids: set[str] | None = None,
) -> list[ShimFile]:
    """Generate shim source for every entry in the mapping tables.

    `skip_type_ids` names messages with an existing hand-written
    facade — the generator won't overwrite them.
    """
    skip = set(skip_type_ids or ())
    out: list[ShimFile] = []

    for tid, cls in sorted(_HEADER_ONLY.items()):
        if tid in skip:
            continue
        out.append(ShimFile(
            class_name=cls,
            header_path=Path(f"igtl{cls}.h"),
            cxx_path=None,   # header-only; no .cxx needed
            hpp_text=_header_only_hpp(tid, cls),
            cxx_text=None,
        ))

    for tid, cls in sorted(_DATA_MESSAGES.items()):
        if tid in skip:
            continue
        out.append(ShimFile(
            class_name=cls,
            header_path=Path(f"igtl{cls}.h"),
            cxx_path=None,   # stub — no .cxx until a real impl lands
            hpp_text=_data_stub_hpp(tid, cls),
            cxx_text=None,
        ))

    return out


def write_shim_files(
    *,
    include_dir: Path,
    skip_type_ids: set[str] | None = None,
) -> list[Path]:
    """Write all shim headers to `include_dir`. Returns the paths written."""
    include_dir.mkdir(parents=True, exist_ok=True)
    written = []
    for sf in all_shim_files(skip_type_ids=skip_type_ids):
        path = include_dir / sf.header_path
        path.write_text(sf.hpp_text)
        written.append(path)
    return written
