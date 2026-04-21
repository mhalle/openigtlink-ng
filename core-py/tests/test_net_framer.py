"""Unit tests for ``oigtl.net.framer``.

Covers:
- Round-trip of a realistic wire buffer (header + body).
- Short-read returns ``None`` without consuming.
- Multiple back-to-back messages in one buffer.
- CRC mismatch raises.
- ``max_body_size`` enforcement before body bytes are required.
"""

from __future__ import annotations

import pytest

from oigtl.net.errors import FramingError
from oigtl.net.framer import HEADER_SIZE, make_v3_framer
from oigtl.runtime import pack_header
from oigtl.runtime.exceptions import CrcMismatchError


def _frame(body: bytes, *, type_id: str = "TRANSFORM",
           device_name: str = "test", timestamp: int = 0) -> bytes:
    # version=1: body is bare (no v2 extended-header region).
    header = pack_header(
        version=1,
        type_id=type_id,
        device_name=device_name,
        timestamp=timestamp,
        body=body,
    )
    return header + body


def test_try_parse_empty_buffer_returns_none() -> None:
    f = make_v3_framer()
    buf = bytearray()
    assert f.try_parse(buf) is None
    assert buf == b""


def test_try_parse_partial_header_returns_none() -> None:
    f = make_v3_framer()
    buf = bytearray(_frame(b"\x00" * 48)[:30])
    assert f.try_parse(buf) is None
    assert len(buf) == 30  # untouched


def test_try_parse_partial_body_returns_none() -> None:
    f = make_v3_framer()
    wire = _frame(b"\x01" * 48)
    buf = bytearray(wire[:HEADER_SIZE + 10])    # header + 10 body bytes
    assert f.try_parse(buf) is None
    assert len(buf) == HEADER_SIZE + 10


def test_try_parse_consumes_full_message() -> None:
    f = make_v3_framer()
    body = bytes(range(48))
    buf = bytearray(_frame(body, type_id="TRANSFORM", device_name="a"))
    inc = f.try_parse(buf)
    assert inc is not None
    assert inc.header.type_id == "TRANSFORM"
    assert inc.header.device_name == "a"
    assert inc.body == body
    assert buf == b""        # prefix consumed


def test_try_parse_back_to_back_messages() -> None:
    f = make_v3_framer()
    body1 = bytes([0xA] * 16)
    body2 = bytes([0xB] * 32)
    buf = bytearray(
        _frame(body1, type_id="STATUS", device_name="d1")
        + _frame(body2, type_id="STRING", device_name="d2")
    )

    inc1 = f.try_parse(buf)
    assert inc1 is not None
    assert inc1.body == body1
    assert inc1.header.device_name == "d1"

    inc2 = f.try_parse(buf)
    assert inc2 is not None
    assert inc2.body == body2
    assert inc2.header.device_name == "d2"

    assert buf == b""


def test_crc_mismatch_raises() -> None:
    f = make_v3_framer()
    body = b"\x00" * 48
    wire = bytearray(_frame(body))
    # Flip a body byte so header.crc disagrees with computed crc.
    wire[-1] ^= 0xFF
    with pytest.raises(CrcMismatchError):
        f.try_parse(wire)


def test_max_body_size_rejects_before_body_bytes_needed() -> None:
    # body_size=48 in the header but we cap at 10. Should raise
    # before ever requiring the body bytes to be present.
    f = make_v3_framer(max_body_size=10)
    body = b"\x00" * 48
    header_only = bytearray(_frame(body)[:HEADER_SIZE])
    with pytest.raises(FramingError):
        f.try_parse(header_only)


def test_max_body_size_accepts_at_limit() -> None:
    f = make_v3_framer(max_body_size=48)
    body = b"\x01" * 48
    buf = bytearray(_frame(body))
    inc = f.try_parse(buf)
    assert inc is not None
    assert len(inc.body) == 48


def test_frame_is_identity_for_v3() -> None:
    f = make_v3_framer()
    wire = _frame(b"\xAB" * 48)
    assert f.frame(wire) == wire


def test_framer_name() -> None:
    assert make_v3_framer().name == "v3"


def test_max_body_size_rejects_negative() -> None:
    with pytest.raises(ValueError):
        make_v3_framer(max_body_size=-1)
