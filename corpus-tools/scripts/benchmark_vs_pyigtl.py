"""Performance benchmark: schema-driven codec vs. pyigtl.

Measures pack, unpack, and round-trip throughput on the same upstream
test vectors. Only compares on message types that pyigtl dispatches
on receive: TRANSFORM, IMAGE, STRING, POINT, POSITION.

Usage::

    uv run --with pyigtl --with numpy python scripts/benchmark_vs_pyigtl.py

Requires pyigtl (with crcmod, numpy). The oigtl-corpus-tools dev deps
include crcmod for its optional CRC-64 fast path.
"""

from __future__ import annotations

import sys
import time
from pathlib import Path

# Locate pyigtl relative to this script
_REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(_REPO / "corpus-tools" / "reference-libs" / "pyigtl"))

from pyigtl.messages import MessageBase  # noqa: E402

from oigtl_corpus_tools.codec.crc64 import crc64  # noqa: E402
from oigtl_corpus_tools.codec.header import (  # noqa: E402
    HEADER_SIZE,
    pack_header,
    unpack_header,
)
from oigtl_corpus_tools.codec.message import (  # noqa: E402
    load_schema,
    pack_body,
    unpack_body,
)
from oigtl_corpus_tools.codec.test_vectors import UPSTREAM_VECTORS  # noqa: E402


def _bench(fn, seconds: float = 1.0, min_iters: int = 100) -> float:
    """Run fn() for ~seconds wall time, return average µs per call."""
    for _ in range(10):
        fn()
    t0 = time.perf_counter()
    iters = 0
    deadline = t0 + seconds
    while time.perf_counter() < deadline or iters < min_iters:
        fn()
        iters += 1
    elapsed = time.perf_counter() - t0
    return (elapsed / iters) * 1e6


def _compare(name: str, data: bytes) -> None:
    print(f"\n{'=' * 72}")
    print(f"  {name}  ({len(data)} bytes)")
    print("=" * 72)

    # Precompute shared state
    header_info = unpack_header(data)
    schema = load_schema(header_info["type"])
    body_values = unpack_body(schema, data[HEADER_SIZE:])

    h = MessageBase.parse_header(data[:HEADER_SIZE])
    pym = MessageBase.create_message(h["message_type"])
    if pym is None:
        print("  pyigtl: type unsupported — skipping")
        return
    pym.unpack(h, data[HEADER_SIZE:])

    def ours_unpack():
        unpack_header(data)
        unpack_body(schema, data[HEADER_SIZE:])

    def ours_pack():
        body = pack_body(schema, body_values)
        pack_header(
            version=header_info["version"],
            type_id=header_info["type"],
            device_name=header_info["device_name"],
            timestamp=header_info["timestamp"],
            body=body,
        )

    def their_unpack():
        h2 = MessageBase.parse_header(data[:HEADER_SIZE])
        m = MessageBase.create_message(h2["message_type"])
        m.unpack(h2, data[HEADER_SIZE:])

    def their_pack():
        pym.pack()

    ours_u = _bench(ours_unpack)
    their_u = _bench(their_unpack)
    ours_p = _bench(ours_pack)
    their_p = _bench(their_pack)

    mb = len(data) / (1024 * 1024)
    print(
        f"  {'':22s} {'ours (µs)':>12s} {'pyigtl (µs)':>12s} {'ratio':>10s}"
    )
    print(
        f"  {'unpack':22s} {ours_u:>12.1f} {their_u:>12.1f} "
        f"{their_u / ours_u:>10.2f}x"
    )
    print(
        f"  {'pack':22s} {ours_p:>12.1f} {their_p:>12.1f} "
        f"{their_p / ours_p:>10.2f}x"
    )
    print(
        f"  {'unpack throughput':22s} "
        f"{mb / (ours_u * 1e-6):>10.1f} MB/s  "
        f"{mb / (their_u * 1e-6):>10.1f} MB/s"
    )


def main() -> int:
    # Types pyigtl dispatches AND can round-trip
    # (POSITION has a pyigtl unpack bug, excluded)
    for name in ("transform", "string", "image"):
        if name in UPSTREAM_VECTORS:
            _compare(name, UPSTREAM_VECTORS[name])

    # Also show CRC-64 alone for context
    body = UPSTREAM_VECTORS["image"][HEADER_SIZE:]
    print(f"\n{'=' * 72}")
    print(f"  CRC-64 ({len(body)} bytes)")
    print("=" * 72)
    t = _bench(lambda: crc64(body))
    print(f"  our crc64 (crcmod fast path if available): {t:.1f} µs")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
