"""Semantic helper tests — pixel_array and data_array."""

from __future__ import annotations

import pytest

pytest.importorskip("numpy")

import numpy as np  # noqa: E402

from oigtl.messages import Image, Ndarray  # noqa: E402
from oigtl.semantic import data_array, pixel_array  # noqa: E402


# ---------------------------------------------------------------------------
# IMAGE
# ---------------------------------------------------------------------------


class TestPixelArray:
    def _make_image(self, w: int, h: int, scalar_type: int, *,
                    endian: int = 1, num_components: int = 1,
                    pixels: bytes | None = None) -> Image:
        if pixels is None:
            dtype = {2: np.int8, 3: np.uint8, 4: ">i2",
                     10: ">f4"}[scalar_type]
            arr = np.arange(w * h * num_components, dtype=dtype)
            pixels = arr.tobytes()
        return Image(
            header_version=2,
            num_components=num_components,
            scalar_type=scalar_type,
            endian=endian,
            coord=1,
            size=[w, h, 1],
            matrix=[1.0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0],
            subvol_offset=[0, 0, 0],
            subvol_size=[w, h, 1],
            pixels=pixels,
        )

    def test_uint8_shape_and_dtype(self):
        img = self._make_image(4, 3, scalar_type=3)
        arr = pixel_array(img)
        assert arr.shape == (1, 3, 4)
        assert arr.dtype == np.uint8
        # values 0..11
        assert list(arr.flatten()) == list(range(12))

    def test_int16_big_endian(self):
        img = self._make_image(2, 2, scalar_type=4, endian=1)
        arr = pixel_array(img)
        assert arr.shape == (1, 2, 2)
        assert arr.dtype.str == ">i2"
        assert list(arr.flatten()) == [0, 1, 2, 3]

    def test_float32_roundtrip(self):
        img = self._make_image(2, 2, scalar_type=10, endian=1)
        arr = pixel_array(img)
        assert arr.shape == (1, 2, 2)
        # Cast to native for value comparison
        vals = arr.astype(np.float32).flatten().tolist()
        assert vals == [0.0, 1.0, 2.0, 3.0]

    def test_multi_component(self):
        # RGB: 2×2 image with 3 components → 12 uint8 values total
        img = self._make_image(2, 2, scalar_type=3, num_components=3)
        arr = pixel_array(img)
        assert arr.shape == (1, 2, 2, 3)

    def test_little_endian(self):
        # Make int16 little-endian bytes, set endian=2
        pixels = np.array([1, 2, 3, 4], dtype="<i2").tobytes()
        img = Image(
            header_version=2, num_components=1, scalar_type=4,
            endian=2, coord=1,
            size=[2, 2, 1],
            matrix=[1.0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0],
            subvol_offset=[0, 0, 0], subvol_size=[2, 2, 1],
            pixels=pixels,
        )
        arr = pixel_array(img)
        assert arr.dtype.str == "<i2"
        assert list(arr.flatten().astype(int)) == [1, 2, 3, 4]

    def test_unknown_scalar_type_raises(self):
        img = self._make_image(2, 2, scalar_type=3)
        img.scalar_type = 99
        with pytest.raises(ValueError, match="unknown scalar_type"):
            pixel_array(img)


# ---------------------------------------------------------------------------
# NDARRAY
# ---------------------------------------------------------------------------


class TestDataArray:
    def test_float64_3d(self):
        arr_in = np.arange(5 * 4 * 3, dtype=">f8").reshape(5, 4, 3)
        msg = Ndarray(
            scalar_type=11, dim=3,
            size=[5, 4, 3],
            data=arr_in.tobytes(),
        )
        arr = data_array(msg)
        assert arr.shape == (5, 4, 3)
        assert arr.dtype.str == ">f8"
        assert np.allclose(arr.astype(np.float64), arr_in.astype(np.float64))

    def test_int16_1d(self):
        raw = np.array([1, 2, 3, 4, 5], dtype=">i2").tobytes()
        msg = Ndarray(scalar_type=4, dim=1, size=[5], data=raw)
        arr = data_array(msg)
        assert arr.shape == (5,)
        assert list(arr.astype(int)) == [1, 2, 3, 4, 5]

    def test_dim_size_mismatch_raises(self):
        raw = np.array([1, 2, 3, 4], dtype=">i2").tobytes()
        msg = Ndarray(scalar_type=4, dim=3, size=[2, 2], data=raw)
        with pytest.raises(ValueError, match="NDARRAY.size length"):
            data_array(msg)
