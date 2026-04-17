"""Variable-count primitive array handling (Phase 2 of the numpy plan).

Exercises the ``oigtl.runtime.arrays`` coercion/pack layer across
construction and round-trip. Splits on whether numpy is installed —
both paths are supposed to be correct; the numpy path additionally
yields ``np.ndarray`` instead of ``array.array``.
"""

from __future__ import annotations

import array

import pytest

from oigtl.messages import Position, Sensor
from oigtl.runtime.arrays import (
    _HAS_NUMPY,
    coerce_variable_array,
    empty_variable_array,
    pack_variable_array,
)


# ---------------------------------------------------------------------------
# Round-trip from construct → pack → unpack → compare
# ---------------------------------------------------------------------------


class TestRoundTrip:
    def test_position_from_list(self):
        p = Position(
            position=[1.0, 2.0, 3.0],
            quaternion=[0.0, 0.5, 0.5, 0.7071067],
        )
        p2 = Position.unpack(p.pack())
        # Values equal within float32 precision. Compare via list to
        # work across both ndarray and array.array runtime containers.
        for a, b in zip(list(p.quaternion), list(p2.quaternion)):
            assert abs(a - b) < 1e-6

    def test_position_three_component_quaternion(self):
        """POSITION body=24 uses a 3-element 'compressed' quaternion."""
        p = Position(
            position=[1.0, 2.0, 3.0],
            quaternion=[0.1, 0.2, 0.3],
        )
        assert len(p.pack()) == 24
        p2 = Position.unpack(p.pack())
        assert len(p2.quaternion) == 3

    def test_sensor_round_trip(self):
        s = Sensor(
            larray=3, status=0, unit=0,
            data=[1.5, 2.5, 3.5],
        )
        b = s.pack()
        s2 = Sensor.unpack(b)
        assert len(s2.data) == 3
        for a, b in zip(list(s.data), list(s2.data)):
            assert abs(a - b) < 1e-12


# ---------------------------------------------------------------------------
# coerce_variable_array direct tests
# ---------------------------------------------------------------------------


class TestCoerce:
    def test_from_list_float32(self):
        arr = coerce_variable_array([0.0, 0.5, 1.0], "float32")
        if _HAS_NUMPY:
            import numpy as np
            assert isinstance(arr, np.ndarray)
            assert arr.dtype == np.dtype(">f4")
        else:
            assert isinstance(arr, array.array)
            assert arr.typecode == "f"
        assert len(arr) == 3

    def test_from_bytes_float32(self):
        """Coerce from raw big-endian wire bytes."""
        # 1.0 as big-endian float32 = 3F 80 00 00
        raw = b"\x3f\x80\x00\x00\x40\x00\x00\x00"  # [1.0, 2.0]
        arr = coerce_variable_array(raw, "float32")
        assert list(arr) == [1.0, 2.0]

    def test_from_bytes_int16(self):
        raw = b"\x00\x01\x00\x02\xff\xff"  # [1, 2, -1]
        arr = coerce_variable_array(raw, "int16")
        assert list(arr) == [1, 2, -1]

    def test_uint8_stays_bytes(self):
        """uint8 arrays are always ``bytes`` regardless of numpy."""
        arr = coerce_variable_array([1, 2, 3], "uint8")
        assert isinstance(arr, bytes)
        assert arr == b"\x01\x02\x03"

    def test_empty_default(self):
        e = empty_variable_array("float64")
        assert len(e) == 0
        e_u8 = empty_variable_array("uint8")
        assert e_u8 == b""


# ---------------------------------------------------------------------------
# pack_variable_array
# ---------------------------------------------------------------------------


class TestPack:
    def test_pack_list(self):
        raw = pack_variable_array([1.0, 2.0], "float32")
        assert raw == b"\x3f\x80\x00\x00\x40\x00\x00\x00"

    def test_pack_bytes_passthrough(self):
        raw = pack_variable_array(b"\x3f\x80\x00\x00", "float32")
        assert raw == b"\x3f\x80\x00\x00"

    def test_pack_array(self):
        a = array.array("h", [1, 2, -1])
        raw = pack_variable_array(a, "int16")
        assert raw == b"\x00\x01\x00\x02\xff\xff"

    @pytest.mark.skipif(not _HAS_NUMPY, reason="numpy not installed")
    def test_pack_ndarray(self):
        import numpy as np
        a = np.array([1.0, 2.0], dtype=np.float32)
        raw = pack_variable_array(a, "float32")
        assert raw == b"\x3f\x80\x00\x00\x40\x00\x00\x00"

    @pytest.mark.skipif(not _HAS_NUMPY, reason="numpy not installed")
    def test_pack_ndarray_native_endian(self):
        """astype handles native→BE even if ndarray came in native-endian."""
        import numpy as np
        a = np.array([1.0], dtype="<f4")  # little-endian
        raw = pack_variable_array(a, "float32")
        assert raw == b"\x3f\x80\x00\x00"


# ---------------------------------------------------------------------------
# Type assertion — what does the user actually see?
# ---------------------------------------------------------------------------


class TestRuntimeType:
    def test_field_type_matches_extra(self):
        """Variable-count primitive field is ndarray with numpy, array
        otherwise; fixed-count stays list; uint8 stays bytes."""
        p = Position(
            position=[1.0, 2.0, 3.0],
            quaternion=[0.0, 0.0, 0.0, 1.0],
        )
        # position is fixed-count → list[float]
        assert isinstance(p.position, list)
        # quaternion is variable-count → ndarray or array.array
        if _HAS_NUMPY:
            import numpy as np
            assert isinstance(p.quaternion, np.ndarray)
        else:
            assert isinstance(p.quaternion, array.array)
