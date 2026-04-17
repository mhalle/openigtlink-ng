"""Round-trip every supported upstream fixture through the typed
Python codec, mirroring core-cpp/tests/upstream_fixtures_test.cpp.

For each fixture we:
1. Use the reference oracle (oigtl_corpus_tools.codec.oracle) to
   parse framing + extract content bytes (this works for both v1
   and v2/v3 messages).
2. Look up the typed message class via REGISTRY.
3. Unpack content into a typed Pydantic instance.
4. Re-pack to bytes.
5. Assert byte-exact equality.

A separate parametrized test asserts that every type_id in
REGISTRY is well-formed (constructible with defaults, packs to a
buffer of the declared body size for fixed-body messages).
"""

from __future__ import annotations

import pytest

from oigtl.messages import REGISTRY
from oigtl_corpus_tools.codec.oracle import verify_wire_bytes
from oigtl_corpus_tools.codec.test_vectors import UPSTREAM_VECTORS


# Map upstream-fixture name → wire type_id of its outer message.
# Upstream uses lowercase fixture filenames; type_ids are uppercase
# and don't always match (TRAJECTORY → TRAJ, COLORTABLE → COLORTABLE
# but our schema also has COLORT, etc.).
_FIXTURE_TYPE_IDS: dict[str, str] = {
    "transform": "TRANSFORM",
    "status": "STATUS",
    "string": "STRING",
    "sensor": "SENSOR",
    "position": "POSITION",
    "image": "IMAGE",
    "colortable": "COLORTABLE",
    "command": "COMMAND",
    "capability": "CAPABILITY",
    "point": "POINT",
    "trajectory": "TRAJ",
    "tdata": "TDATA",
    "imgmeta": "IMGMETA",
    "lbmeta": "LBMETA",
    "videometa": "VIDEOMETA",
    "bind": "BIND",
    "polydata": "POLYDATA",
    "ndarray": "NDARRAY",
    "transformFormat2": "TRANSFORM",
    "positionFormat2": "POSITION",
    "tdataFormat2": "TDATA",
    "trajectoryFormat2": "TRAJ",
    "commandFormat2": "COMMAND",
}


@pytest.mark.parametrize(
    "fixture_name,type_id",
    sorted(_FIXTURE_TYPE_IDS.items()),
)
def test_typed_round_trip(fixture_name: str, type_id: str) -> None:
    """Body content round-trips byte-for-byte through the typed class."""
    if fixture_name not in UPSTREAM_VECTORS:
        pytest.skip(f"fixture {fixture_name!r} not extracted")
    if type_id not in REGISTRY:
        pytest.skip(f"type_id {type_id!r} not in registry")

    wire = UPSTREAM_VECTORS[fixture_name]
    framing = verify_wire_bytes(wire)
    assert framing.ok, framing.error
    assert framing.type_id == type_id

    msg_cls = REGISTRY[type_id]
    msg = msg_cls.unpack(framing.body if isinstance(framing.body, bytes)
                         else _content_bytes_from_oracle(wire))
    repacked = msg.pack()

    expected_content = _content_bytes_from_oracle(wire)
    assert repacked == expected_content, (
        f"{fixture_name}: typed round-trip mismatch "
        f"({len(expected_content)}B expected, {len(repacked)}B actual)"
    )


def _content_bytes_from_oracle(wire: bytes) -> bytes:
    """Extract the content region from a wire message via the oracle.

    Reuses the framing parser to slice [ext_header, content,
    metadata]. For v1 messages, content is the full body.
    """
    # Re-implement the slice locally so the test doesn't depend on
    # private oracle internals. We need: header parse → optional
    # extended header → content slice.
    from oigtl_corpus_tools.codec.header import HEADER_SIZE, unpack_header

    header = unpack_header(wire)
    body = wire[HEADER_SIZE:HEADER_SIZE + header["body_size"]]
    if header["version"] < 2:
        return body
    # v2/v3: skip ext_header_size bytes at the start, then strip
    # (metadata_header_size + metadata_size) at the end.
    import struct
    ext_header_size, mh, ms, _msg_id = struct.unpack_from(">HHII", body, 0)
    metadata_total = mh + ms
    return body[ext_header_size:len(body) - metadata_total]


@pytest.mark.parametrize("type_id", sorted(REGISTRY.keys()))
def test_default_construction(type_id: str) -> None:
    """Every message class can be default-constructed and packs cleanly."""
    cls = REGISTRY[type_id]
    msg = cls()
    body = msg.pack()
    if hasattr(cls, "BODY_SIZE"):
        assert len(body) == cls.BODY_SIZE, (
            f"{type_id}: default body size {len(body)} != declared "
            f"{cls.BODY_SIZE}"
        )


def test_registry_is_complete() -> None:
    """REGISTRY contains every type_id we expect (no silent omissions)."""
    # Spot-check: 84 schemas → 84 registry entries. The exact count
    # is asserted as a regression guard against codegen losing a type.
    assert len(REGISTRY) == 84


def test_class_attributes() -> None:
    """TYPE_ID matches the registry key for every entry."""
    for type_id, cls in REGISTRY.items():
        assert cls.TYPE_ID == type_id, (
            f"{cls.__name__}.TYPE_ID={cls.TYPE_ID!r} != registry key {type_id!r}"
        )
