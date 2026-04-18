#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""Python side of the core-c parity test.

Writes the canonical wire bytes of a named case to stdout using the
`oigtl_corpus_tools.codec` reference codec (the same codec core-py
sits on top of).  The C parity emitter produces the same case via
core-c's generated C, and the ctest wrapper diffs the two byte
streams via `cmake -E compare_files`.

Invoked by ctest with a single argument — the case name. Case
names must match the C emitter's exactly.
"""
from __future__ import annotations

import struct
import sys

# Reference codec lives in corpus-tools; when ctest invokes us, the
# repo root's parent is on sys.path via the test command so imports
# resolve without any install step.


def emit_transform() -> bytes:
    """Matches emitter.c::emit_transform — 12 float32s, i+1 * 0.5."""
    buf = bytearray(48)
    for i in range(12):
        v = (i + 1) * 0.5
        struct.pack_into(">f", buf, i * 4, v)
    return bytes(buf)


def emit_status() -> bytes:
    """uint16 code + int64 subcode + 20-byte null-padded error_name
    + trailing null-terminated status_message."""
    body = bytearray()
    body += struct.pack(">H", 7)
    body += struct.pack(">q", -42)
    error_name = b"HW_FAULT"
    body += error_name + b"\x00" * (20 - len(error_name))
    msg = b"coolant pressure low"
    body += msg
    body += b"\x00"
    return bytes(body)


def _pos_body(pos: list[float], quat: list[float]) -> bytes:
    body = bytearray()
    for p in pos:
        body += struct.pack(">f", p)
    for q in quat:
        body += struct.pack(">f", q)
    return bytes(body)


def emit_position_full() -> bytes:
    return _pos_body([1.0, 2.0, 3.0], [0.25, 0.5, 0.75, 1.0])


def emit_position_only() -> bytes:
    return _pos_body([4.0, 5.0, 6.0], [])


def emit_sensor() -> bytes:
    """uint8 larray + uint8 status + uint64 unit + N*float64 data."""
    body = bytearray()
    body += struct.pack(">B", 3)             # larray
    body += struct.pack(">B", 0)             # status
    body += struct.pack(">Q", 0x0102030405060708)  # unit
    for v in (1.5, -2.25, 0.125):
        body += struct.pack(">d", v)
    return bytes(body)


EMITTERS = {
    "transform":     emit_transform,
    "status":        emit_status,
    "position_full": emit_position_full,
    "position_only": emit_position_only,
    "sensor":        emit_sensor,
}


def main(argv: list[str]) -> int:
    if len(argv) != 2 or argv[1] not in EMITTERS:
        print(
            "usage: parity_emitter_py.py <case>\n"
            f"cases: {', '.join(sorted(EMITTERS))}",
            file=sys.stderr,
        )
        return 2
    sys.stdout.buffer.write(EMITTERS[argv[1]]())
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
