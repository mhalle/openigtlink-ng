"""Phase-1 acceptance test: TRANSFORM round-trips against the upstream fixture.

Mirrors the C++ test (``core-cpp/tests/upstream_fixtures_test.cpp``)
on the same byte buffer. We use ``oigtl_corpus_tools.codec.test_vectors``
to load the fixture, since it already extracts upstream bytes.
"""

from __future__ import annotations

import pytest

from oigtl.messages import Transform
from oigtl.runtime import HEADER_SIZE, pack_header, unpack_header
from oigtl_corpus_tools.codec.test_vectors import UPSTREAM_VECTORS


@pytest.fixture
def transform_wire() -> bytes:
    return UPSTREAM_VECTORS["transform"]


def test_unpack_returns_typed_instance(transform_wire: bytes) -> None:
    body = transform_wire[HEADER_SIZE:]
    tx = Transform.unpack(body)
    assert isinstance(tx, Transform)
    assert isinstance(tx.matrix, list)
    assert len(tx.matrix) == 12
    assert all(isinstance(v, float) for v in tx.matrix)


def test_class_constants() -> None:
    assert Transform.TYPE_ID == "TRANSFORM"
    assert Transform.BODY_SIZE == 48


def test_body_round_trips_byte_exact(transform_wire: bytes) -> None:
    body = transform_wire[HEADER_SIZE:]
    tx = Transform.unpack(body)
    assert tx.pack() == body


def test_full_message_round_trips(transform_wire: bytes) -> None:
    """Header + body round-trip: parse, repack body, repack header, compare."""
    header = unpack_header(transform_wire)
    assert header.type_id == "TRANSFORM"
    assert header.body_size == Transform.BODY_SIZE

    body = transform_wire[HEADER_SIZE:HEADER_SIZE + header.body_size]
    tx = Transform.unpack(body)
    repacked_body = tx.pack()
    assert repacked_body == body

    repacked_header = pack_header(
        version=header.version,
        type_id=header.type_id,
        device_name=header.device_name,
        timestamp=header.timestamp,
        body=repacked_body,
    )
    assert repacked_header + repacked_body == transform_wire


def test_default_matrix_packs_to_zeros() -> None:
    tx = Transform()
    body = tx.pack()
    assert body == b"\x00" * 48


def test_validation_rejects_wrong_length() -> None:
    from pydantic import ValidationError

    with pytest.raises(ValidationError):
        Transform(matrix=[0.0] * 11)  # 11 elements, schema says 12
