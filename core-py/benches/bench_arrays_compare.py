"""Compare coerce_variable_array across runtime containers.

Sweeps (element_type × size) and reports µs/op for four paths over
raw big-endian wire bytes (what the codec produces):

  1. list[T] via struct.unpack         — pre-Phase-1 representation
  2. array.array + byteswap            — stdlib fallback (no numpy)
  3. np.frombuffer                     — numpy zero-copy view
  4. coerce_variable_array (installed) — what the typed layer actually does
     under the current environment (numpy if available, else array.array)

Run with:
    uv run --project core-py python core-py/benches/bench_arrays_compare.py
"""

from __future__ import annotations

import array
import struct
import sys
import time
from typing import Callable

from oigtl.runtime import arrays as oarr


PRIMITIVES = [
    ("int16",   "h", ">h"),
    ("uint16",  "H", ">H"),
    ("int32",   "i", ">i"),
    ("uint32",  "I", ">I"),
    ("int64",   "q", ">q"),
    ("uint64",  "Q", ">Q"),
    ("float32", "f", ">f"),
    ("float64", "d", ">d"),
]

SIZES = [100, 1_000, 10_000, 100_000]


def bench(fn: Callable[[], object], seconds: float = 0.4,
          min_iters: int = 100) -> float:
    for _ in range(5):
        fn()
    t0 = time.perf_counter()
    iters = 0
    deadline = t0 + seconds
    while time.perf_counter() < deadline or iters < min_iters:
        fn()
        iters += 1
    return ((time.perf_counter() - t0) / iters) * 1e6


def make_wire_bytes(element_type: str, n: int) -> bytes:
    """Synthesize n elements of big-endian wire bytes for a primitive."""
    code, be_fmt = next((c, f) for (et, c, f) in PRIMITIVES if et == element_type)
    if element_type.startswith("float"):
        values = [float(i) / 7.0 for i in range(n)]
    elif element_type.startswith("int"):
        values = [((i * 31) % 2**15) - 2**14 for i in range(n)]
    else:
        values = [(i * 31) % 2**15 for i in range(n)]
    return struct.pack(">" + be_fmt[1] * n, *values)


def bench_one(element_type: str, n: int) -> dict[str, float]:
    raw = make_wire_bytes(element_type, n)
    be_fmt = next(f for (et, c, f) in PRIMITIVES if et == element_type)
    code = next(c for (et, c, f) in PRIMITIVES if et == element_type)
    fmt_n = ">" + be_fmt[1] * n

    results: dict[str, float] = {}

    # 1. list[T] via struct.unpack (what the library emitted pre-Phase-1)
    results["list_via_struct"] = bench(
        lambda: list(struct.unpack(fmt_n, raw))
    )

    # 2. array.array + byteswap (stdlib fallback)
    def via_array():
        a = array.array(code)
        a.frombytes(raw)
        if sys.byteorder == "little":
            a.byteswap()
        return a
    results["array_byteswap"] = bench(via_array)

    # 3. np.frombuffer zero-copy
    try:
        import numpy as np
        dtype = ">" + code_to_npy(code)
        results["np_frombuffer"] = bench(
            lambda: np.frombuffer(raw, dtype=dtype)
        )
    except ImportError:
        results["np_frombuffer"] = float("nan")

    # 4. The library path
    results["coerce_installed"] = bench(
        lambda: oarr.coerce_variable_array(raw, element_type)
    )

    return results


def code_to_npy(code: str) -> str:
    return {
        "b": "i1", "B": "u1",
        "h": "i2", "H": "u2",
        "i": "i4", "I": "u4",
        "q": "i8", "Q": "u8",
        "f": "f4", "d": "f8",
    }[code]


def fmt_us(us: float) -> str:
    if us != us:  # NaN
        return "   n/a"
    if us < 10:
        return f"{us:6.2f}"
    if us < 1000:
        return f"{us:6.1f}"
    return f"{us:6.0f}"


def main() -> int:
    print("oigtl variable-primitive array coercion — cross-type sweep")
    print("=" * 80)
    print(f"numpy available: {oarr._HAS_NUMPY}")
    print(f"coerce_installed path: "
          f"{'np.ndarray' if oarr._HAS_NUMPY else 'array.array'}")
    print()
    print("Times in µs per call. Input: raw big-endian wire bytes (N elements).")
    print()

    header = (
        f"{'type':<9}{'N':>8}  "
        f"{'list+struct':>12}{'array+swap':>12}"
        f"{'np.frombuf':>12}{'coerce()':>12}"
    )
    print(header)
    print("-" * len(header))

    for et, code, be in PRIMITIVES:
        for n in SIZES:
            r = bench_one(et, n)
            print(
                f"{et:<9}{n:>8}  "
                f"{fmt_us(r['list_via_struct']):>12}"
                f"{fmt_us(r['array_byteswap']):>12}"
                f"{fmt_us(r['np_frombuffer']):>12}"
                f"{fmt_us(r['coerce_installed']):>12}"
            )
        print()

    # Speedup summary at N=10K
    print("\nSpeedup at N=10,000 vs baseline list[T] via struct.unpack:")
    print("-" * 60)
    print(f"{'type':<9}{'array+swap':>14}{'np.frombuf':>14}{'coerce()':>14}")
    for et, code, be in PRIMITIVES:
        r = bench_one(et, 10_000)
        base = r["list_via_struct"]
        arr_s = base / r["array_byteswap"] if r["array_byteswap"] else 0
        np_s = base / r["np_frombuffer"] if r["np_frombuffer"] == r["np_frombuffer"] else 0
        co_s = base / r["coerce_installed"] if r["coerce_installed"] else 0
        print(f"{et:<9}{arr_s:>13.1f}x{np_s:>13.1f}x{co_s:>13.1f}x")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
