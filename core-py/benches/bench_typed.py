"""Microbenchmark for the typed Python codec.

Reports µs/op and MB/s for pack, unpack, and full parse_message
across the same fixtures the C++ bench measures (TRANSFORM, IMAGE,
VIDEOMETA) plus a raw `oracle.verify_wire_bytes` baseline.

Three layers compared:

1. **Reference codec** (oigtl_corpus_tools.codec.fields.unpack_fields)
   — dict-based, no Pydantic. The fastest Python path.

2. **Typed codec** (oigtl.messages.Transform.unpack)
   — adds Pydantic validation. The default user-facing API.

3. **Full parse_message** — header + CRC + framing + dispatch + typed
   construct. The "I just want a typed message from these bytes"
   one-call API.

Run with:
    uv run --project core-py python core-py/benches/bench_typed.py
"""

from __future__ import annotations

import time
from typing import Callable

from oigtl.messages import Image, REGISTRY, Transform, Videometa, parse_message
from oigtl.messages.image import _FIELDS as _IMAGE_FIELDS
from oigtl.messages.transform import _FIELDS as _TRANSFORM_FIELDS
from oigtl.messages.videometa import _FIELDS as _VIDEOMETA_FIELDS
from oigtl.runtime.oracle import verify_wire_bytes

from oigtl_corpus_tools.codec.fields import pack_fields, unpack_fields
from oigtl_corpus_tools.codec.test_vectors import UPSTREAM_VECTORS


def bench(fn: Callable[[], object], seconds: float = 1.0,
          min_iters: int = 1000) -> float:
    """Return average µs per call after a warmup run."""
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


def _content_bytes(wire: bytes) -> bytes:
    """Slice out the schema-content region, handling v3 framing."""
    import struct
    from oigtl_corpus_tools.codec.header import HEADER_SIZE, unpack_header
    h = unpack_header(wire)
    body = wire[HEADER_SIZE:HEADER_SIZE + h["body_size"]]
    if h["version"] < 2:
        return body
    eh, mh, ms, _ = struct.unpack_from(">HHII", body, 0)
    return body[eh:len(body) - mh - ms]


def row(label: str, us: float, bytes_: int) -> None:
    mb_per_s = (bytes_ / (us * 1e-6)) / (1024 * 1024)
    print(f"  {label:<32s} {us:10.3f} µs/op   {mb_per_s:10.1f} MB/s")


def bench_message(name: str, wire: bytes, msg_cls, fields_literal) -> None:
    print(f"\n{name}  ({len(wire)} wire bytes)")
    print(f"  {'operation':<32s} {'time':>10s}        {'throughput':>10s}")
    print(f"  {'-' * 70}")

    content = _content_bytes(wire)

    # Layer 1: reference dict-based codec
    row("ref unpack (dict)",
        bench(lambda: unpack_fields(fields_literal, content)),
        len(content))
    msg = msg_cls.unpack(content)
    values = msg.model_dump()
    row("ref pack (dict)",
        bench(lambda: pack_fields(fields_literal, values)),
        len(content))

    # Layer 2: typed Pydantic codec
    row("typed unpack (Pydantic)",
        bench(lambda: msg_cls.unpack(content)),
        len(content))
    row("typed pack (Pydantic)",
        bench(lambda: msg.pack()),
        len(content))

    # Layer 3: full parse_message (header + CRC + framing + dispatch)
    row("parse_message (full pipeline)",
        bench(lambda: parse_message(wire)),
        len(wire))


def bench_oracle() -> None:
    print("\nOracle on IMAGE (full pipeline + round-trip verify)")
    print(f"  {'-' * 70}")
    wire = UPSTREAM_VECTORS["image"]
    row("verify_wire_bytes", bench(lambda: verify_wire_bytes(wire)),
        len(wire))


def synthesize_image(width: int, height: int) -> tuple[bytes, Image]:
    """Build a wire IMAGE message of width×height grayscale uint8 pixels.

    Returns (full_wire_bytes, prepacked_typed_instance) so the bench
    can exercise unpack on the wire and pack on the typed instance.
    """
    from oigtl.runtime import pack_header

    msg = Image(
        header_version=2,
        num_components=1,
        scalar_type=3,         # uint8
        endian=1,
        coord=1,
        size=[width, height, 1],
        matrix=[1.0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0],
        subvol_offset=[0, 0, 0],
        subvol_size=[width, height, 1],
        pixels=bytes(range(256)) * ((width * height) // 256)
                + bytes((width * height) % 256),
    )
    body = msg.pack()
    header = pack_header(
        version=1, type_id="IMAGE", device_name="BenchCam",
        timestamp=0, body=body,
    )
    return header + body, msg


def main() -> int:
    print("oigtl typed Python codec — microbenchmark")
    print("=========================================")
    print(f"Registry contains {len(REGISTRY)} codecs")

    bench_message("TRANSFORM", UPSTREAM_VECTORS["transform"],
                  Transform, _TRANSFORM_FIELDS)
    bench_message("IMAGE", UPSTREAM_VECTORS["image"],
                  Image, _IMAGE_FIELDS)
    bench_message("VIDEOMETA (v3, struct array)",
                  UPSTREAM_VECTORS["videometa"],
                  Videometa, _VIDEOMETA_FIELDS)
    bench_oracle()

    # Realistic image sizes — the upstream fixture is 50×50, which
    # is two orders of magnitude smaller than what real medical
    # imaging carries. 640×480 is the practical floor; 1920×1080
    # is common for tracked endoscopy / AR overlays.
    print("\n" + "=" * 70)
    print("Realistic image sizes")
    print("=" * 70)
    for w, h, label in [
        (640, 480,  "VGA   (640×480 grayscale)"),
        (1024, 768, "XGA   (1024×768 grayscale)"),
        (1920, 1080,"FHD   (1920×1080 grayscale)"),
    ]:
        wire, msg = synthesize_image(w, h)
        body_len = len(wire) - 58
        body = wire[58:]
        print(f"\n{label}  ({body_len:,} body bytes)")
        print(f"  {'-' * 70}")

        # Reference dict-codec (no Pydantic). Best Python can do
        # given the codec returns list[int] for uint8 arrays.
        row("ref unpack (dict only)",
            bench(lambda: unpack_fields(_IMAGE_FIELDS, body), seconds=0.5),
            body_len)
        # Typed default (with Pydantic validation of each int).
        row("typed unpack (full validation)",
            bench(lambda: Image.unpack(body), seconds=0.5),
            body_len)
        # Typed via model_construct — skips per-field validation.
        # Equivalent to "I trust this data, just give me the typed
        # object". Pydantic intends this for trusted internal state.
        values = unpack_fields(_IMAGE_FIELDS, body)
        row("typed unpack (model_construct)",
            bench(lambda: Image.model_construct(**values), seconds=0.5),
            body_len)
        row("typed pack",
            bench(lambda: msg.pack(), seconds=0.5), body_len)
        row("parse_message (full pipeline)",
            bench(lambda: parse_message(wire), seconds=0.5),
            len(wire))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
