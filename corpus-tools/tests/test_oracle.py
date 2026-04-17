"""Oracle test suite — systematically verifies every upstream test vector.

For each test vector extracted from the upstream C headers, the oracle
runs the full pipeline: header parse → CRC verify → schema unpack →
repack → byte comparison. A passing test means the reference codec
produces byte-identical output to the upstream C library for that
message.

Vectors that are expected to fail (e.g. v2/v3 extended-header
messages that the body-only codec doesn't yet strip) are marked xfail
with a reason, so the suite stays green while documenting the gap.
"""

from __future__ import annotations

import struct

import pytest

from oigtl_corpus_tools.codec.header import HEADER_SIZE, unpack_header
from oigtl_corpus_tools.codec.oracle import verify_wire_bytes
from oigtl_corpus_tools.codec.test_vectors import UPSTREAM_VECTORS


# ---------------------------------------------------------------------------
# Vectors that need special handling
# ---------------------------------------------------------------------------

# These vectors use v2/v3 extended headers or have known format
# differences that the body-only codec doesn't yet handle. They're
# expected to fail at the unpack or round-trip stage, not at CRC.
_XFAIL_VECTORS: dict[str, str] = {
    "rtpwrapper": "RTP/UDP fragmentation wrapper, not a standard OpenIGTLink message",
}


# ---------------------------------------------------------------------------
# Discovery
# ---------------------------------------------------------------------------

def _vector_ids() -> list[str]:
    """Return sorted list of test vector names."""
    return sorted(UPSTREAM_VECTORS.keys())


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------


class TestUpstreamVectorDiscovery:
    def test_vectors_found(self):
        """We should find at least 15 test vectors (after reassembly)."""
        assert len(UPSTREAM_VECTORS) >= 15, (
            f"only found {len(UPSTREAM_VECTORS)} vectors"
        )

    def test_vector_names(self):
        """Spot-check that key vectors are present."""
        for name in ("transform", "status", "point", "sensor", "ndarray", "polydata", "bind"):
            assert name in UPSTREAM_VECTORS, f"missing vector: {name}"


class TestUpstreamVectorCRC:
    """CRC verification — should pass for ALL vectors regardless of
    whether we can unpack/round-trip them."""

    @pytest.mark.parametrize("name", _vector_ids())
    def test_crc_valid(self, name: str):
        data = UPSTREAM_VECTORS[name]
        if len(data) < HEADER_SIZE:
            pytest.skip("too short for header")
        result = verify_wire_bytes(
            data, check_crc=True, check_round_trip=False
        )
        # CRC should pass even if unpack fails
        assert "CRC mismatch" not in result.error, result.error


class TestUpstreamVectorRoundTrip:
    """Full oracle round-trip — unpack, repack, compare bytes."""

    @pytest.mark.parametrize("name", _vector_ids())
    def test_round_trip(self, name: str):
        if name in _XFAIL_VECTORS:
            pytest.xfail(_XFAIL_VECTORS[name])

        data = UPSTREAM_VECTORS[name]
        result = verify_wire_bytes(data)
        assert result.ok, (
            f"oracle FAIL for {name} (type={result.type_id}): {result.error}"
        )
        assert result.round_trip_ok


class TestUpstreamVectorFieldValues:
    """Spot-check specific field values from known vectors."""

    def test_transform_matrix_count(self):
        result = verify_wire_bytes(UPSTREAM_VECTORS["transform"])
        assert result.ok
        assert len(result.body["matrix"]) == 12

    def test_status_fields(self):
        result = verify_wire_bytes(UPSTREAM_VECTORS["status"])
        assert result.ok
        assert result.body["code"] == 15
        assert result.body["error_name"] == "ACTUATOR_DISABLED"

    def test_sensor_has_data(self):
        result = verify_wire_bytes(UPSTREAM_VECTORS["sensor"])
        assert result.ok
        assert "data" in result.body
        assert "larray" in result.body

    def test_string_has_content(self):
        result = verify_wire_bytes(UPSTREAM_VECTORS["string"])
        assert result.ok
        assert len(result.body["value"]) > 0

    def test_point_has_elements(self):
        result = verify_wire_bytes(UPSTREAM_VECTORS["point"])
        assert result.ok
        assert len(result.body["points"]) > 0

    def test_position_has_position(self):
        result = verify_wire_bytes(UPSTREAM_VECTORS["position"])
        assert result.ok
        assert len(result.body["position"]) == 3

    def test_capability_has_supported_types(self):
        result = verify_wire_bytes(UPSTREAM_VECTORS["capability"])
        assert result.ok
        assert len(result.body["supported_types"]) > 0

    def test_trajectory_has_trajectories(self):
        result = verify_wire_bytes(UPSTREAM_VECTORS["trajectory"])
        assert result.ok
        assert len(result.body["trajectories"]) > 0

    def test_ndarray_round_trip(self):
        result = verify_wire_bytes(UPSTREAM_VECTORS["ndarray"])
        assert result.ok
        assert result.body["scalar_type"] == 11  # float64
        assert result.body["dim"] == 3
        # size is variable-count uint16 → raw wire bytes.
        # 3 dims × 2 bytes each = 6 bytes big-endian: [5,4,3].
        assert result.body["size"] == struct.pack(">3H", 5, 4, 3)

    def test_bind_structure(self):
        result = verify_wire_bytes(UPSTREAM_VECTORS["bind"])
        assert result.ok
        assert result.body["ncmessages"] > 0
        assert len(result.body["header_entries"]) == result.body["ncmessages"]

    def test_polydata_structure(self):
        result = verify_wire_bytes(UPSTREAM_VECTORS["polydata"])
        assert result.ok
        assert result.body["npoints"] > 0
        assert len(result.body["points"]) == result.body["npoints"]


class TestExtendedHeaderAndMetadata:
    """Verify the oracle correctly handles v2+ framing (ext header + metadata)."""

    def test_transform_format2_has_extended_header(self):
        """Format2 vectors declare version=2 and carry an extended header."""
        if "transformFormat2" not in UPSTREAM_VECTORS:
            pytest.skip("transformFormat2 vector not discovered")
        result = verify_wire_bytes(UPSTREAM_VECTORS["transformFormat2"])
        assert result.ok, result.error
        assert result.header["version"] == 2
        assert result.extended_header is not None
        assert result.extended_header["ext_header_size"] == 12
        # Content schema still decodes correctly
        assert len(result.body["matrix"]) == 12

    def test_transform_format2_metadata_decoded(self):
        if "transformFormat2" not in UPSTREAM_VECTORS:
            pytest.skip("transformFormat2 vector not discovered")
        result = verify_wire_bytes(UPSTREAM_VECTORS["transformFormat2"])
        assert result.ok, result.error
        # transformFormat2 fixture carries 2 metadata entries
        assert len(result.metadata) == 2
        # Each entry is (key, encoding, value_bytes)
        for key, encoding, value in result.metadata:
            assert isinstance(key, str)
            assert isinstance(encoding, int)
            assert isinstance(value, (bytes, bytearray))

    def test_videometa_now_works(self):
        """VIDEOMETA fixture uses version=2 + v3 extended header — oracle handles it."""
        result = verify_wire_bytes(UPSTREAM_VECTORS["videometa"])
        assert result.ok, result.error
        assert len(result.body["videos"]) == 3

    def test_v1_message_has_no_extended_header(self):
        """v1 messages should have extended_header=None."""
        result = verify_wire_bytes(UPSTREAM_VECTORS["transform"])
        assert result.ok
        assert result.header["version"] == 1
        assert result.extended_header is None
        assert result.metadata == []

    def test_colortable_legacy_schema(self):
        """The legacy 'COLORTABLE' wire type_id now decodes via colortable_legacy.json."""
        result = verify_wire_bytes(UPSTREAM_VECTORS["colortable"])
        assert result.ok, result.error
        assert result.type_id == "COLORTABLE"
        assert "table" in result.body
