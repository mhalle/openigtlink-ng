"""Tests for the reference codec.

Verifies header parsing, CRC, field unpacking, pack/unpack symmetry,
and round-trip against upstream test vectors.
"""

from __future__ import annotations

import struct

import pytest

from oigt_corpus_tools.codec.crc64 import crc64
from oigt_corpus_tools.codec.header import HEADER_SIZE, pack_header, unpack_header, verify_crc
from oigt_corpus_tools.codec.message import (
    load_schema,
    pack_body,
    pack_message,
    unpack_body,
    unpack_message,
)


# ---------------------------------------------------------------------------
# CRC-64 — check against a known value
# ---------------------------------------------------------------------------


class TestCRC64:
    def test_ecma182_check_value(self):
        """CRC-64 of "123456789" matches the OpenIGTLink table variant."""
        assert crc64(b"123456789") == 0x6C40DF5F0B497347

    def test_empty(self):
        assert crc64(b"") == 0

    def test_chaining(self):
        """CRC can be computed in chunks."""
        full = crc64(b"hello world")
        chained = crc64(b" world", crc64(b"hello"))
        assert full == chained


# ---------------------------------------------------------------------------
# Upstream TRANSFORM test vector (from igtl_test_data_transform.h)
# ---------------------------------------------------------------------------

# Complete wire message: 58-byte header + 48-byte body
TRANSFORM_WIRE = bytes([
    # --- Header (58 bytes) ---
    0x00, 0x01,                                             # version = 1
    0x54, 0x52, 0x41, 0x4E, 0x53, 0x46, 0x4F, 0x52,
    0x4D, 0x00, 0x00, 0x00,                                 # "TRANSFORM"
    0x44, 0x65, 0x76, 0x69, 0x63, 0x65, 0x4E, 0x61,
    0x6D, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,                                 # "DeviceName"
    0x00, 0x00, 0x00, 0x00, 0x49, 0x96, 0x02, 0xD4,       # timestamp
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30,       # body_size = 48
    0xF6, 0xDD, 0x2B, 0x8E, 0xB4, 0xDF, 0x6D, 0xD2,       # CRC
    # --- Body (48 bytes = 12 float32) ---
    0xBF, 0x74, 0x73, 0xCD,  # matrix[0]
    0x3E, 0x49, 0x59, 0xE6,  # matrix[1]
    0xBE, 0x63, 0xDD, 0x98,  # matrix[2]
    0xBE, 0x49, 0x59, 0xE6,  # matrix[3]
    0x3E, 0x12, 0x49, 0x1B,  # matrix[4]
    0x3F, 0x78, 0x52, 0xD6,  # matrix[5]
    0x3E, 0x63, 0xDD, 0x98,  # matrix[6]
    0x3F, 0x78, 0x52, 0xD6,  # matrix[7]
    0xBD, 0xC8, 0x30, 0xAE,  # matrix[8]
    0x42, 0x38, 0x36, 0x60,  # matrix[9]
    0x41, 0x9B, 0xC4, 0x67,  # matrix[10]
    0x42, 0x38, 0x36, 0x60,  # matrix[11]
])


class TestHeader:
    def test_unpack_transform_header(self):
        hdr = unpack_header(TRANSFORM_WIRE)
        assert hdr["version"] == 1
        assert hdr["type"] == "TRANSFORM"
        assert hdr["device_name"] == "DeviceName"
        assert hdr["body_size"] == 48

    def test_crc_matches(self):
        hdr = unpack_header(TRANSFORM_WIRE)
        body = TRANSFORM_WIRE[HEADER_SIZE:]
        verify_crc(hdr, body)  # should not raise

    def test_crc_mismatch_raises(self):
        hdr = unpack_header(TRANSFORM_WIRE)
        bad_body = b"\x00" * 48
        with pytest.raises(ValueError, match="CRC mismatch"):
            verify_crc(hdr, bad_body)


class TestTransformRoundTrip:
    def test_unpack_transform(self):
        header, body = unpack_message(TRANSFORM_WIRE)
        assert header["type"] == "TRANSFORM"
        assert len(body["matrix"]) == 12
        # First matrix element: 0xBF7473CD → approximately -0.9555
        assert abs(body["matrix"][0] - struct.unpack(">f", bytes([0xBF, 0x74, 0x73, 0xCD]))[0]) < 1e-6

    def test_pack_unpack_symmetry(self):
        """Unpack the wire bytes, repack, and compare."""
        header, body = unpack_message(TRANSFORM_WIRE)
        repacked = pack_message(
            "TRANSFORM",
            device_name=header["device_name"],
            values=body,
            version=header["version"],
            timestamp=header["timestamp"],
        )
        assert repacked == TRANSFORM_WIRE


# ---------------------------------------------------------------------------
# STATUS test vector (from igtl_test_data_status.h)
# ---------------------------------------------------------------------------

# Upstream test vector from igtl_test_data_status.h
STATUS_WIRE = bytes([
    # --- Header ---
    0x00, 0x01,
    0x53, 0x54, 0x41, 0x54, 0x55, 0x53, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,                                 # "STATUS"
    0x44, 0x65, 0x76, 0x69, 0x63, 0x65, 0x4E, 0x61,
    0x6D, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x49, 0x96, 0x02, 0xD4,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36,       # body_size = 54
    0x98, 0xEE, 0x43, 0xEE, 0xD8, 0xE4, 0x31, 0xCF,       # CRC
    # --- Body (54 bytes) ---
    0x00, 0x0F,                                             # code = 15
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A,       # subcode = 10
    0x41, 0x43, 0x54, 0x55, 0x41, 0x54, 0x4F, 0x52,       # error_name "ACTUATOR_DISABLED"
    0x5F, 0x44, 0x49, 0x53, 0x41, 0x42, 0x4C, 0x45,
    0x44, 0x00, 0x00, 0x00,
    # status_message: "Actuator A is disabled." + null
    0x41, 0x63, 0x74, 0x75, 0x61, 0x74, 0x6F, 0x72,
    0x20, 0x41, 0x20, 0x69, 0x73, 0x20, 0x64, 0x69,
    0x73, 0x61, 0x62, 0x6C, 0x65, 0x64, 0x2E, 0x00,
])


class TestStatusRoundTrip:
    def test_unpack_status(self):
        header, body = unpack_message(STATUS_WIRE)
        assert header["type"] == "STATUS"
        assert body["code"] == 15
        assert body["subcode"] == 10
        assert body["error_name"] == "ACTUATOR_DISABLED"
        # trailing_string field
        assert "Actuator A is disabled" in body["status_message"]

    def test_pack_unpack_symmetry(self):
        header, body = unpack_message(STATUS_WIRE)
        repacked = pack_message(
            "STATUS",
            device_name=header["device_name"],
            values=body,
            version=header["version"],
            timestamp=header["timestamp"],
        )
        assert repacked == STATUS_WIRE


# ---------------------------------------------------------------------------
# POSITION test — variable-size (28-byte full quaternion)
# ---------------------------------------------------------------------------

POSITION_WIRE = bytes([
    # --- Header ---
    0x00, 0x01,
    0x50, 0x4F, 0x53, 0x49, 0x54, 0x49, 0x4F, 0x4E,
    0x00, 0x00, 0x00, 0x00,                                 # "POSITION"
    0x44, 0x65, 0x76, 0x69, 0x63, 0x65, 0x4E, 0x61,
    0x6D, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x49, 0x96, 0x02, 0xD4,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C,       # body_size = 28
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       # CRC placeholder
    # --- Body (28 bytes: 3 position + 4 quaternion) ---
    0x42, 0xF6, 0xE9, 0x79,  # position[0] = 123.456
    0x42, 0xF6, 0xE9, 0x79,  # position[1] = 123.456
    0x42, 0xF6, 0xE9, 0x79,  # position[2] = 123.456
    0x3F, 0x2A, 0xAA, 0xAB,  # quaternion[0] = ~0.667
    0x3E, 0x2A, 0xAA, 0xAB,  # quaternion[1] = ~0.167
    0x3D, 0x2A, 0xAA, 0xAB,  # quaternion[2] = ~0.042
    0x3F, 0x40, 0x00, 0x00,  # quaternion[3] = 0.75
])

# Fix the CRC in the header
_pos_body = POSITION_WIRE[HEADER_SIZE:]
_pos_crc = crc64(_pos_body)
POSITION_WIRE = POSITION_WIRE[:50] + struct.pack(">Q", _pos_crc) + _pos_body


class TestPositionRoundTrip:
    def test_unpack_position(self):
        header, body = unpack_message(POSITION_WIRE)
        assert header["type"] == "POSITION"
        assert len(body["position"]) == 3
        assert len(body["quaternion"]) == 4  # full quaternion

    def test_pack_unpack_symmetry(self):
        header, body = unpack_message(POSITION_WIRE)
        repacked = pack_message(
            "POSITION",
            device_name=header["device_name"],
            values=body,
            version=header["version"],
            timestamp=header["timestamp"],
        )
        assert repacked == POSITION_WIRE


# ---------------------------------------------------------------------------
# Empty-body message (GET_TRANS)
# ---------------------------------------------------------------------------


class TestEmptyBody:
    def test_get_trans_round_trip(self):
        wire = pack_message("GET_TRANS", device_name="Tracker")
        header, body = unpack_message(wire)
        assert header["type"] == "GET_TRANS"
        assert header["device_name"] == "Tracker"
        assert body == {}
        assert header["body_size"] == 0


# ---------------------------------------------------------------------------
# Schema loading
# ---------------------------------------------------------------------------


class TestSchemaLoading:
    def test_load_known_type(self):
        schema = load_schema("TRANSFORM")
        assert schema["message_type"] == "TRANSFORM"

    def test_load_unknown_type_raises(self):
        with pytest.raises(KeyError):
            load_schema("NONEXISTENT")

    def test_all_schemas_loadable(self):
        """Every .json in spec/schemas/ should be loadable by its type_id."""
        from oigt_corpus_tools.paths import find_repo_root, schemas_dir
        import json

        sdir = schemas_dir(find_repo_root())
        for path in sorted(sdir.glob("*.json")):
            with open(path) as f:
                obj = json.load(f)
            tid = obj.get("type_id")
            if tid:
                loaded = load_schema(tid)
                assert loaded["type_id"] == tid, f"mismatch for {path.name}"
