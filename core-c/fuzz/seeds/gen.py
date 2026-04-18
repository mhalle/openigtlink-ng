#!/usr/bin/env python3
# /// script
# requires-python = ">=3.9"
# dependencies = []
# ///
"""Generate a small libFuzzer seed corpus for the core-c fuzz targets.

Invoked by CMake at configure / build time; writes one subdirectory
per fuzz target under the output directory given as `--out`, each
containing a handful of canonical inputs that libFuzzer's mutator
will fan out from.

The seeds cover:

* One \"valid\" canonical input per target (matches the parity-emitter
  case for the same type).
* A few obvious boundary cases: all-zeros, malformed sizes, missing
  terminators. Most finds come from libFuzzer's mutator — the seeds
  are hints, not exhaustive coverage.

Script is stdlib-only so it runs unmodified under any Python 3 on
any CI runner. No uv dependency at runtime.
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


# -------- TRANSFORM ---------------------------------------------------------

def transform_valid() -> bytes:
    """Matches the parity-emitter canonical case."""
    return b"".join(struct.pack(">f", (i + 1) * 0.5) for i in range(12))


def transform_zeros() -> bytes:
    return b"\x00" * 48


def transform_too_short() -> bytes:
    return b"\x00" * 12


# -------- STATUS ------------------------------------------------------------

def status_minimal() -> bytes:
    body = bytearray()
    body += struct.pack(">H", 1)         # code
    body += struct.pack(">q", 0)         # subcode
    body += b"\x00" * 20                 # error_name (null-padded)
    body += b"\x00"                      # empty status_message + terminator
    return bytes(body)


def status_long_message() -> bytes:
    body = bytearray()
    body += struct.pack(">H", 7)
    body += struct.pack(">q", -42)
    name = b"HW_FAULT"
    body += name + b"\x00" * (20 - len(name))
    body += b"coolant pressure low"
    body += b"\x00"
    return bytes(body)


def status_no_null_term() -> bytes:
    """Trailing byte is 'X' instead of NUL — unpack must reject."""
    body = bytearray()
    body += struct.pack(">H", 1)
    body += struct.pack(">q", 0)
    body += b"\x00" * 20
    body += b"oops"
    return bytes(body)  # no NUL


# -------- POSITION ----------------------------------------------------------

def _position_body(pos: list[float], quat: list[float]) -> bytes:
    buf = bytearray()
    for p in pos:
        buf += struct.pack(">f", p)
    for q in quat:
        buf += struct.pack(">f", q)
    return bytes(buf)


def position_12() -> bytes:
    return _position_body([1.0, 2.0, 3.0], [])


def position_24() -> bytes:
    return _position_body([1.0, 2.0, 3.0], [0.1, 0.2, 0.3])


def position_28() -> bytes:
    return _position_body([1.0, 2.0, 3.0], [0.25, 0.5, 0.75, 1.0])


def position_malformed() -> bytes:
    """Not in the {12,24,28} allowed set — unpack must reject."""
    return _position_body([1.0, 2.0, 3.0], [0.1, 0.2])


# -------- SENSOR ------------------------------------------------------------

def sensor_larray_0() -> bytes:
    """Empty data array — 10-byte header only."""
    body = bytearray()
    body += struct.pack(">B", 0)         # larray
    body += struct.pack(">B", 0)         # status
    body += struct.pack(">Q", 0)         # unit
    return bytes(body)


def sensor_larray_3() -> bytes:
    body = bytearray()
    body += struct.pack(">B", 3)
    body += struct.pack(">B", 0)
    body += struct.pack(">Q", 0x0102030405060708)
    for v in (1.5, -2.25, 0.125):
        body += struct.pack(">d", v)
    return bytes(body)


def sensor_larray_mismatch() -> bytes:
    """Claimed larray=10 but only 3 float64s follow — unpack rejects."""
    body = bytearray()
    body += struct.pack(">B", 10)
    body += struct.pack(">B", 0)
    body += struct.pack(">Q", 0)
    for v in (1.0, 2.0, 3.0):
        body += struct.pack(">d", v)
    return bytes(body)


# -------- Layout ------------------------------------------------------------

SEEDS: dict[str, list[tuple[str, bytes]]] = {
    "transform": [
        ("valid_01.bin",     transform_valid()),
        ("zeros.bin",        transform_zeros()),
        ("too_short.bin",    transform_too_short()),
    ],
    "status": [
        ("minimal.bin",      status_minimal()),
        ("long_message.bin", status_long_message()),
        ("no_null_term.bin", status_no_null_term()),
    ],
    "position": [
        ("12_bytes.bin",     position_12()),
        ("24_bytes.bin",     position_24()),
        ("28_bytes.bin",     position_28()),
        ("malformed.bin",    position_malformed()),
    ],
    "sensor": [
        ("larray_0.bin",     sensor_larray_0()),
        ("larray_3.bin",     sensor_larray_3()),
        ("larray_mismatch.bin", sensor_larray_mismatch()),
    ],
}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, required=True,
                    help="Output directory; one subfolder per target.")
    args = ap.parse_args()

    args.out.mkdir(parents=True, exist_ok=True)
    for target, seeds in SEEDS.items():
        tdir = args.out / target
        tdir.mkdir(parents=True, exist_ok=True)
        for name, payload in seeds:
            (tdir / name).write_bytes(payload)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
