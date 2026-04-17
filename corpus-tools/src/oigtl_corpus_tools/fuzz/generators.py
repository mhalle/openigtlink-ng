"""Candidate byte-sequence generators for the differential fuzzer.

Three generator strategies:

- :func:`random_bytes` — uniform-random bytes with a Pareto-distributed
  length, so most inputs are small and occasional inputs are large.
- :func:`mutate_fixture` — take an upstream fixture and apply one
  deterministic mutation (bit flip, byte insert, byte delete, chunk
  duplicate, length-field tamper).
- :func:`structured_header` — synthesize a plausible header (valid
  framing, ASCII type_id, correct CRC) with a random body of random
  length; the fuzzer then decides whether the receiver accepts.

All generators take a :class:`random.Random` seeded by the runner so
a test run is fully reproducible from its seed.
"""

from __future__ import annotations

import random
import struct
from typing import Callable, Iterator

from oigtl_corpus_tools.codec.crc64 import crc64
from oigtl_corpus_tools.codec.header import HEADER_SIZE
from oigtl_corpus_tools.codec.test_vectors import UPSTREAM_VECTORS


# ---------------------------------------------------------------------------
# Random-bytes generator
# ---------------------------------------------------------------------------


def _pareto_length(rng: random.Random, scale: int = 64, max_len: int = 8192) -> int:
    """Return an integer length from a Pareto distribution.

    Parameters chosen so the median input is ~96 bytes (a typical
    header+small-body) but the tail reaches into KB territory. ``max_len``
    caps the distribution so a runaway value can't OOM the fuzzer.
    """
    # Pareto(alpha=1.5, xm=scale); numpy isn't a runtime dep so we
    # invert the CDF by hand.
    u = rng.random()
    # u ∈ [0, 1); avoid log(0)
    if u < 1e-12:
        u = 1e-12
    value = scale * (u ** (-1.0 / 1.5))
    return min(int(value), max_len)


def random_bytes(rng: random.Random, *, max_len: int = 8192) -> bytes:
    """One uniform-random byte sequence with Pareto-distributed length."""
    n = _pareto_length(rng, max_len=max_len)
    return bytes(rng.randrange(256) for _ in range(n))


# ---------------------------------------------------------------------------
# Mutate-a-fixture generator
# ---------------------------------------------------------------------------

_MUTATION_KINDS: tuple[str, ...] = (
    "bit_flip",
    "byte_insert",
    "byte_delete",
    "chunk_duplicate",
    "length_tamper",
    "crc_tamper",
)


def _mutate_once(rng: random.Random, data: bytes, kind: str) -> bytes:
    """Apply one mutation `kind` to `data`; return a fresh buffer."""
    if not data:
        return bytes(rng.randrange(256) for _ in range(rng.randrange(1, 32)))

    if kind == "bit_flip":
        arr = bytearray(data)
        idx = rng.randrange(len(arr))
        arr[idx] ^= 1 << rng.randrange(8)
        return bytes(arr)

    if kind == "byte_insert":
        idx = rng.randrange(len(data) + 1)
        return data[:idx] + bytes([rng.randrange(256)]) + data[idx:]

    if kind == "byte_delete":
        idx = rng.randrange(len(data))
        return data[:idx] + data[idx + 1:]

    if kind == "chunk_duplicate":
        # Duplicate an 8..64 byte chunk at a random position.
        chunk_size = min(rng.randrange(8, 65), len(data))
        src = rng.randrange(len(data) - chunk_size + 1)
        chunk = data[src:src + chunk_size]
        dst = rng.randrange(len(data) + 1)
        return data[:dst] + chunk + data[dst:]

    if kind == "length_tamper":
        # Rewrite the header body_size field to a random small
        # delta. Targets the framing layer's length handling.
        if len(data) < HEADER_SIZE:
            return _mutate_once(rng, data, "bit_flip")
        arr = bytearray(data)
        # body_size lives at offset 42 (uint64 BE). Pick a value
        # near the real one so the fuzzer sometimes hits the exact
        # boundary (off-by-one, short-by-one).
        current = int.from_bytes(arr[42:50], "big")
        delta = rng.choice([-2, -1, 0, 1, 2, 32, -32])
        new = max(0, current + delta)
        arr[42:50] = new.to_bytes(8, "big")
        return bytes(arr)

    if kind == "crc_tamper":
        # Flip a bit in the CRC field so the receiver must reject.
        if len(data) < HEADER_SIZE:
            return _mutate_once(rng, data, "bit_flip")
        arr = bytearray(data)
        arr[50 + rng.randrange(8)] ^= 1 << rng.randrange(8)
        return bytes(arr)

    raise ValueError(f"unknown mutation kind: {kind!r}")


def mutate_fixture(
    rng: random.Random,
    fixtures: dict[str, bytes] | None = None,
) -> bytes:
    """Return a fixture with one random mutation applied."""
    source = fixtures if fixtures is not None else UPSTREAM_VECTORS
    name = rng.choice(list(source.keys()))
    data = source[name]
    kind = rng.choice(_MUTATION_KINDS)
    return _mutate_once(rng, data, kind)


# ---------------------------------------------------------------------------
# Structured-header generator
# ---------------------------------------------------------------------------

_TYPE_IDS: tuple[str, ...] = (
    "TRANSFORM", "STATUS", "IMAGE", "POSITION", "SENSOR",
    "POINT", "STRING", "NDARRAY", "BIND", "TDATA",
    # Some invalid-ish ones to exercise the unknown-type path.
    "NOPE", "FOOBAR", "X",
)


def structured_header(rng: random.Random) -> bytes:
    """Synthesize a plausible header with a random body.

    Produces a header with a chosen version, type_id, random body
    content, and a correctly-computed CRC. The body is random bytes
    — usually malformed content for the chosen type_id, but always
    framing-consistent. Exercises the body-decode path after the
    outer frame accepts.
    """
    version = rng.choice((0, 1, 2, 3, 4, 99))  # include invalid ones
    type_id = rng.choice(_TYPE_IDS).encode("ascii").ljust(12, b"\x00")[:12]
    device_name = b"FuzzDev".ljust(20, b"\x00")

    body_len = rng.choice((0, 12, 24, 28, 48, 54, rng.randrange(0, 512)))
    body = bytes(rng.randrange(256) for _ in range(body_len))

    timestamp = rng.getrandbits(64)
    body_size = body_len
    # Half the time produce a valid CRC, half a random one — the
    # fuzzer should see both accept (when content happens to parse)
    # and CRC-rejection paths.
    if rng.random() < 0.5:
        crc = crc64(body)
    else:
        crc = rng.getrandbits(64)

    header = (
        struct.pack(">H", version)
        + type_id
        + device_name
        + struct.pack(">Q", timestamp)
        + struct.pack(">Q", body_size)
        + struct.pack(">Q", crc)
    )
    return header + body


# ---------------------------------------------------------------------------
# Dispatcher
# ---------------------------------------------------------------------------


GeneratorFn = Callable[[random.Random], bytes]

GENERATORS: dict[str, GeneratorFn] = {
    "random": random_bytes,
    "mutate": mutate_fixture,
    "structured": structured_header,
}


def iter_candidates(
    rng: random.Random,
    generators: list[str],
    iterations: int,
) -> Iterator[tuple[str, bytes]]:
    """Yield (generator_name, bytes) pairs up to *iterations*.

    Rotates through the requested generators round-robin so each gets
    an equal share of iterations regardless of individual speed.
    """
    fns = [(name, GENERATORS[name]) for name in generators]
    for i in range(iterations):
        name, fn = fns[i % len(fns)]
        yield name, fn(rng)
