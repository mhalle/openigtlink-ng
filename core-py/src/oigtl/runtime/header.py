"""Typed wrapper around the 58-byte OpenIGTLink message header.

The reference codec exposes the header as a raw ``dict[str, Any]``;
here we wrap it in a frozen Pydantic model so callers get
attribute access, validation, and IDE help. Symmetric to
``oigtl::runtime::Header`` in core-cpp.
"""

from __future__ import annotations

from pydantic import BaseModel, ConfigDict

from oigtl_corpus_tools.codec.header import HEADER_SIZE
from oigtl_corpus_tools.codec.header import pack_header as _pack_header
from oigtl_corpus_tools.codec.header import unpack_header as _unpack_header

__all__ = ["HEADER_SIZE", "Header", "unpack_header", "pack_header"]


class Header(BaseModel):
    """A parsed 58-byte OpenIGTLink message header."""

    model_config = ConfigDict(frozen=True)

    version: int
    type_id: str
    device_name: str
    timestamp: int
    body_size: int
    crc: int


def unpack_header(data: bytes) -> Header:
    """Parse a 58-byte header into a :class:`Header`.

    Raises :class:`ValueError` if *data* is shorter than 58 bytes
    (kept as ValueError to match the underlying reference codec —
    callers using the oracle layer get a typed
    :class:`~oigtl.runtime.exceptions.ShortBufferError` instead).
    """
    raw = _unpack_header(data)
    return Header(
        version=raw["version"],
        type_id=raw["type"],
        device_name=raw["device_name"],
        timestamp=raw["timestamp"],
        body_size=raw["body_size"],
        crc=raw["crc"],
    )


#: Minimum size of a v2/v3 extended-header region — see
#: ``spec/schemas/framing_header.json`` and the parallel constant
#: in :mod:`oigtl.codec`.
EXT_HEADER_MIN_SIZE = 12


def pack_header(
    *,
    version: int,
    type_id: str,
    device_name: str,
    timestamp: int,
    body: bytes,
    validate: bool = True,
) -> bytes:
    """Build a 58-byte header for *body*. CRC is computed automatically.

    Invariant (enabled by default via ``validate=True``): when
    ``version >= 2``, *body* must begin with a plausible extended
    header region. Concretely:

    - ``len(body) >= EXT_HEADER_MIN_SIZE``, and
    - the first 12 bytes decode as
      ``(ext_header_size, metadata_header_size, metadata_size,
      message_id)`` with ``ext_header_size`` in
      ``[EXT_HEADER_MIN_SIZE, len(body)]`` and
      ``metadata_header_size + metadata_size`` fitting in the
      remaining bytes.

    This catches a real bug class (declaring ``version=2`` while
    emitting a v1-style bare body, which a strict v2 receiver will
    misparse) at the authoring site instead of letting it cross the
    network. The cross-runtime test against core-cpp's strict
    v2 parser is the canonical regression case.

    Raises :class:`ValueError` when the invariant fails.

    Pass ``validate=False`` to skip the invariant. Intended only
    for fuzzers / oracle binaries that deliberately emit malformed
    or adversarial wire bytes; production code must never set it.
    """
    if validate and version >= 2:
        _check_v2_body_starts_with_ext_header(version, body)
    return _pack_header(
        version=version,
        type_id=type_id,
        device_name=device_name,
        timestamp=timestamp,
        body=body,
    )


def _check_v2_body_starts_with_ext_header(version: int, body: bytes) -> None:
    """Guardrail for :func:`pack_header` — see its docstring."""
    if len(body) < EXT_HEADER_MIN_SIZE:
        raise ValueError(
            f"pack_header(version={version}) requires body to begin "
            f"with a {EXT_HEADER_MIN_SIZE}-byte extended-header "
            f"region; got {len(body)} bytes. If you meant to emit "
            f"v1 framing (no extended header), pass version=1 "
            f"instead; if you're deliberately emitting malformed "
            f"bytes from a fuzzer, pass validate=False."
        )
    # Same layout _extract_content reads on the receive side.
    ext_header_size = int.from_bytes(body[0:2], "big")
    meta_header_size = int.from_bytes(body[2:4], "big")
    meta_size = int.from_bytes(body[4:8], "big")
    if ext_header_size < EXT_HEADER_MIN_SIZE or ext_header_size > len(body):
        raise ValueError(
            f"pack_header(version={version}): ext_header_size "
            f"{ext_header_size} is out of range [{EXT_HEADER_MIN_SIZE}, "
            f"{len(body)}]. See pack_header's docstring for the v2 "
            f"body layout."
        )
    metadata_total = meta_header_size + meta_size
    if metadata_total > len(body) - ext_header_size:
        raise ValueError(
            f"pack_header(version={version}): declared metadata "
            f"region ({metadata_total} bytes) overruns body after "
            f"the ext header ({len(body) - ext_header_size} bytes "
            f"remain)."
        )
