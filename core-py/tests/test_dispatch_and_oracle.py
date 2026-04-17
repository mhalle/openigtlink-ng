"""Tests for the high-level typed dispatch and oracle wrappers."""

from __future__ import annotations

import pytest

from oigtl.messages import (
    REGISTRY,
    Status,
    Transform,
    extract_content_bytes,
    parse_message,
)
from oigtl.runtime.exceptions import (
    CrcMismatchError,
    UnknownMessageTypeError,
)
from oigtl.runtime.oracle import VerifyResult, verify_wire_bytes
from oigtl_corpus_tools.codec.test_vectors import UPSTREAM_VECTORS


def test_parse_message_returns_typed_instance() -> None:
    msg = parse_message(UPSTREAM_VECTORS["transform"])
    assert isinstance(msg, Transform)
    assert len(msg.matrix) == 12


def test_parse_message_status() -> None:
    msg = parse_message(UPSTREAM_VECTORS["status"])
    assert isinstance(msg, Status)
    assert isinstance(msg.code, int)
    assert isinstance(msg.error_name, str)


def test_parse_message_v3_format2() -> None:
    """Format2 fixtures have extended header — content slice must be correct."""
    msg = parse_message(UPSTREAM_VECTORS["transformFormat2"])
    assert isinstance(msg, Transform)
    assert len(msg.matrix) == 12


def test_parse_message_round_trip() -> None:
    """The decoded typed instance packs back to the original content bytes."""
    wire = UPSTREAM_VECTORS["sensor"]
    msg = parse_message(wire)
    type_id, content = extract_content_bytes(wire)
    assert type_id == "SENSOR"
    assert msg.pack() == content


def test_parse_message_rejects_bad_crc() -> None:
    wire = bytearray(UPSTREAM_VECTORS["transform"])
    wire[58] ^= 0x01  # Flip a bit in the body
    with pytest.raises(CrcMismatchError):
        parse_message(bytes(wire))


def test_parse_message_unknown_type_id() -> None:
    """An empty registry should raise UnknownMessageTypeError."""
    with pytest.raises(UnknownMessageTypeError):
        parse_message(UPSTREAM_VECTORS["transform"], registry={})


def test_extract_content_bytes_v1() -> None:
    type_id, content = extract_content_bytes(UPSTREAM_VECTORS["transform"])
    assert type_id == "TRANSFORM"
    assert len(content) == 48


def test_extract_content_bytes_v3() -> None:
    type_id, content = extract_content_bytes(
        UPSTREAM_VECTORS["transformFormat2"]
    )
    assert type_id == "TRANSFORM"
    assert len(content) == 48  # TRANSFORM body is fixed regardless of version


def test_oracle_returns_typed_result() -> None:
    result = verify_wire_bytes(UPSTREAM_VECTORS["transform"])
    assert isinstance(result, VerifyResult)
    assert result.ok
    assert result.round_trip_ok
    assert result.header is not None
    assert result.header.type_id == "TRANSFORM"
    assert result.header.version == 1
    assert result.extended_header is None  # v1 message
    assert result.metadata == []


def test_oracle_v3_message() -> None:
    result = verify_wire_bytes(UPSTREAM_VECTORS["videometa"])
    assert result.ok
    assert result.header.version == 2
    assert result.extended_header is not None
    assert result.extended_header.ext_header_size == 12


def test_oracle_bad_crc() -> None:
    wire = bytearray(UPSTREAM_VECTORS["transform"])
    wire[58] ^= 0x01
    result = verify_wire_bytes(bytes(wire))
    assert not result.ok
    assert "CRC" in result.error or "crc" in result.error
