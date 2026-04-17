"""Microbenchmark for variable-count primitive arrays (numpy vs array.array).

Exercises SENSOR.data at 100 / 1K / 10K float64 elements across three
tiers:

- ``list[float]`` (what the library emitted before the numpy plan)
- ``array.array`` (no-numpy fallback)
- ``np.ndarray`` (``[numpy]`` extra)

Run with:
    uv run --project core-py python core-py/benches/bench_numpy.py
"""

from __future__ import annotations

import array
import struct
import time
from typing import Callable

from oigtl.messages import Sensor
from oigtl.runtime import arrays as oarr
from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields


def bench(fn: Callable[[], object], seconds: float = 0.5,
          min_iters: int = 200) -> float:
    for _ in range(5):
        fn()
    t0 = time.perf_counter()
    iters = 0
    deadline = t0 + seconds
    while time.perf_counter() < deadline or iters < min_iters:
        fn()
        iters += 1
    return ((time.perf_counter() - t0) / iters) * 1e6


def _sensor_body(n: int) -> bytes:
    """Synthesize a SENSOR body with n float64 samples."""
    # larray (uint8), status (uint8), unit (uint64), data[larray] float64
    # larray field is uint8 so max 255 — we fake it by manually writing
    # the wire (bypassing the schema validation that would cap count).
    # For benchmarking purposes we just need the dict/bytes pair.
    header = struct.pack(">BBQ", min(n, 255), 0, 0)
    data = struct.pack(">" + "d" * n, *[float(i) for i in range(n)])
    return header + data


def row(label: str, us: float, items: int) -> None:
    per_item_ns = us * 1000.0 / items if items else 0.0
    print(f"  {label:<40s} {us:10.2f} µs   {per_item_ns:8.1f} ns/elem")


def main() -> int:
    print("oigtl numpy-native bulk arrays — microbenchmark")
    print("================================================")
    print(f"numpy available: {oarr._HAS_NUMPY}")
    print()

    sizes = [100, 1_000, 10_000]

    for n in sizes:
        print(f"\nSENSOR.data with {n} float64 elements")
        print("  " + "-" * 70)

        body = _sensor_body(n)
        _FIELDS = Sensor.__module__  # not actually used; loaded below
        from oigtl.messages.sensor import _FIELDS as SENSOR_FIELDS

        # ref codec (returns bytes for variable primitives after Phase 1)
        row("ref unpack (bytes)",
            bench(lambda: unpack_fields(SENSOR_FIELDS, body)), n)

        # Typed library (coerces to ndarray or array.array via validator)
        row("typed unpack (coerce)",
            bench(lambda: Sensor.unpack(body)), n)

        # Reconstruct for pack benchmarks
        msg = Sensor.unpack(body)
        row("typed pack",
            bench(lambda: msg.pack()), n)

        # Pure coercion cost — how much of the typed unpack delta is us?
        raw = body[10:]  # skip 10-byte prefix to get raw float64 bytes
        row("coerce_variable_array(bytes)",
            bench(lambda: oarr.coerce_variable_array(raw, "float64")), n)

        # For comparison: building a list[float] from the same bytes
        fmt = ">" + "d" * n
        row("list[float] via struct.unpack",
            bench(lambda: list(struct.unpack(fmt, raw))), n)

        if oarr._HAS_NUMPY:
            import numpy as np
            row("np.frombuffer (zero-copy)",
                bench(lambda: np.frombuffer(raw, dtype=">f8")), n)
        else:
            code = oarr._ARRAY_CODE["float64"]
            def via_array():
                a = array.array(code)
                a.frombytes(raw)
                a.byteswap()
                return a
            row("array.array + byteswap",
                bench(via_array), n)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
