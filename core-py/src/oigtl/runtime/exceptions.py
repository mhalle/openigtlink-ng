"""Typed exception hierarchy for the wire codec.

Mirrors the C++ ``oigtl::error`` family in
``core-cpp/include/oigtl/runtime/error.hpp``.

The reference codec in ``oigtl_corpus_tools.codec`` raises plain
:class:`ValueError` for everything; we translate at the boundary
in :mod:`oigtl.runtime.oracle` so callers of the typed library see
a structured exception hierarchy and can ``except CrcMismatchError``
without string-matching.
"""

from __future__ import annotations


class ProtocolError(Exception):
    """Base for any wire-level decoding failure."""


class ShortBufferError(ProtocolError):
    """A read would have exceeded the available bytes."""


class CrcMismatchError(ProtocolError):
    """Header-declared CRC does not match the body's computed CRC."""


class MalformedMessageError(ProtocolError):
    """Wire bytes were structurally invalid (size mismatches, bad framing)."""


class UnknownMessageTypeError(ProtocolError):
    """No codec is registered for the message's wire type_id."""
