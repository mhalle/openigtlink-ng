"""Tests for :mod:`oigtl.codec` — the pure wire codec.

Exercises the public ``unpack_*`` / ``pack_*`` surface over:

- built-in message types (round-trip of hand-constructed messages)
- the ``loose=True`` / ``loose=False`` split for unknown type_ids
- CRC verification on/off
- truncation and trailing-bytes detection

The transport-level receive paths (client, server, ws_client) are
covered by their own suites; here we validate the codec layer in
isolation so a regression shows up with minimum surface area.
"""

from __future__ import annotations

import pytest

from oigtl import (
    HEADER_SIZE,
    RawBody,
    pack_envelope,
    pack_header,
    unpack_envelope,
    unpack_header,
    unpack_message,
)
from oigtl.messages import Status, Transform
from oigtl.runtime.envelope import Envelope
from oigtl.runtime.exceptions import (
    CrcMismatchError,
    MalformedMessageError,
    ShortBufferError,
    UnknownMessageTypeError,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _wrap(msg, *, type_id: str, device_name: str = "dev",
          timestamp: int = 0, version: int = 1) -> bytes:
    """Build a full wire message around *msg*'s body bytes.

    Defaults to ``version=1`` because these helpers produce a bare
    body (no v2 extended-header region). ``pack_header``'s
    invariant rejects ``version>=2`` with a bare body.
    """
    body = msg.pack()
    header = pack_header(
        version=version,
        type_id=type_id,
        device_name=device_name,
        timestamp=timestamp,
        body=body,
    )
    return header + body


def _transform() -> Transform:
    # OpenIGTLink TRANSFORM body is a 3x4 (12-element) affine matrix:
    # the implicit [0,0,0,1] bottom row is omitted.
    return Transform(
        matrix=[1.0, 0.0, 0.0,
                0.0, 1.0, 0.0,
                0.0, 0.0, 1.0,
                11.0, 22.0, 33.0],
    )


def _status() -> Status:
    return Status(
        code=1,
        sub_code=0,
        error_name="",
        status_message="ok",
    )


# ---------------------------------------------------------------------------
# Round-trip — the property that justifies everything else.
# ---------------------------------------------------------------------------


def test_unpack_envelope_then_pack_is_byte_identical_transform():
    wire = _wrap(_transform(), type_id="TRANSFORM")
    env = unpack_envelope(wire)
    assert isinstance(env.body, Transform)
    assert env.header.type_id == "TRANSFORM"
    assert pack_envelope(env) == wire


def test_unpack_envelope_then_pack_is_byte_identical_status():
    wire = _wrap(_status(), type_id="STATUS", device_name="Tracker1",
                 timestamp=1_700_000_000_000_000_000)
    env = unpack_envelope(wire)
    assert isinstance(env.body, Status)
    assert env.body.code == 1
    assert env.header.device_name == "Tracker1"
    assert pack_envelope(env) == wire


def test_unpack_header_round_trip():
    wire = _wrap(_transform(), type_id="TRANSFORM")
    header = unpack_header(wire[:HEADER_SIZE])
    assert header.type_id == "TRANSFORM"
    assert header.body_size == len(wire) - HEADER_SIZE


def test_unpack_message_matches_unpack_envelope():
    """Two-step form (streaming callers) agrees with single-step form."""
    wire = _wrap(_transform(), type_id="TRANSFORM")
    header = unpack_header(wire[:HEADER_SIZE])
    body = wire[HEADER_SIZE:HEADER_SIZE + header.body_size]

    env_two_step = unpack_message(header, body)
    env_one_step = unpack_envelope(wire)

    assert env_two_step == env_one_step


# ---------------------------------------------------------------------------
# Loose / strict on unknown type_ids
# ---------------------------------------------------------------------------


def _wire_with_fabricated_type(type_id: str, body: bytes) -> bytes:
    # version=1 because the body is bare (no v2 extended-header
    # region) — matches pack_header's invariant.
    header = pack_header(
        version=1,
        type_id=type_id,
        device_name="x",
        timestamp=0,
        body=body,
    )
    return header + body


def test_unknown_type_id_strict_raises():
    wire = _wire_with_fabricated_type("NOSUCHTYPE", b"\x00\x01\x02\x03")
    with pytest.raises(UnknownMessageTypeError):
        unpack_envelope(wire, loose=False)


def test_unknown_type_id_loose_returns_rawbody():
    wire = _wire_with_fabricated_type("NOSUCHTYPE", b"\x00\x01\x02\x03")
    env = unpack_envelope(wire, loose=True)
    assert isinstance(env.body, RawBody)
    assert env.body.type_id == "NOSUCHTYPE"
    assert env.body.body == b"\x00\x01\x02\x03"


def test_rawbody_repacks_to_original_wire():
    wire = _wire_with_fabricated_type("NOSUCHTYPE", b"hello")
    env = unpack_envelope(wire, loose=True)
    assert pack_envelope(env) == wire


# ---------------------------------------------------------------------------
# CRC verification
# ---------------------------------------------------------------------------


def test_bad_crc_raises_when_verified():
    wire = bytearray(_wrap(_transform(), type_id="TRANSFORM"))
    # The CRC lives in the last 8 bytes of the 58-byte header.
    wire[HEADER_SIZE - 1] ^= 0xFF
    with pytest.raises(CrcMismatchError):
        unpack_envelope(bytes(wire))


def test_bad_crc_silent_when_verify_disabled():
    wire = bytearray(_wrap(_transform(), type_id="TRANSFORM"))
    wire[HEADER_SIZE - 1] ^= 0xFF
    # No raise — caller explicitly opted out.
    env = unpack_envelope(bytes(wire), verify_crc=False)
    assert isinstance(env.body, Transform)


# ---------------------------------------------------------------------------
# Framing validity
# ---------------------------------------------------------------------------


def test_too_short_for_header_raises():
    with pytest.raises(ShortBufferError):
        unpack_envelope(b"\x00" * (HEADER_SIZE - 1))


def test_truncated_body_raises():
    wire = _wrap(_transform(), type_id="TRANSFORM")
    with pytest.raises(ShortBufferError):
        unpack_envelope(wire[:-1])


def test_trailing_bytes_rejected():
    wire = _wrap(_transform(), type_id="TRANSFORM")
    with pytest.raises(MalformedMessageError):
        unpack_envelope(wire + b"\x99")


def test_unpack_message_rejects_body_length_mismatch():
    wire = _wrap(_transform(), type_id="TRANSFORM")
    header = unpack_header(wire[:HEADER_SIZE])
    body = wire[HEADER_SIZE:HEADER_SIZE + header.body_size]
    # Drop one byte from the body — header still says the original size.
    with pytest.raises(MalformedMessageError):
        unpack_message(header, body[:-1])


# ---------------------------------------------------------------------------
# pack_envelope determinism
# ---------------------------------------------------------------------------


def test_pack_envelope_recomputes_crc_from_body():
    """Even if the envelope carries a stale CRC, pack_envelope produces
    a wire with the correct CRC derived from the current body."""
    wire = _wrap(_transform(), type_id="TRANSFORM")
    env = unpack_envelope(wire)

    # Clone the envelope with a bogus header CRC — pack_envelope should
    # ignore the stale value and recompute.
    stale = Envelope(
        header=env.header.model_copy(update={"crc": 0xDEADBEEFDEADBEEF}),
        body=env.body,
    )
    repacked = pack_envelope(stale)

    # Round-trips through the strict decoder: CRC is valid.
    env2 = unpack_envelope(repacked)
    assert env2.body == env.body
    # And the repacked bytes are byte-identical to the original wire.
    assert repacked == wire
