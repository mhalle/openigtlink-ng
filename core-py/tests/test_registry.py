"""Tests for :mod:`oigtl.messages.registry` and extension registration.

The registry is the pivot that makes built-in types and user-supplied
extension types indistinguishable to the codec. These tests exercise
both halves: confirming the built-ins are all visible through the
public API, and proving that third-party registrations follow
exactly the same decode path.

Each test that mutates the registry cleans up after itself via a
fixture. The registry is a process-wide dict, so leaking a
registration from one test into another would produce
order-dependent failures.
"""

from __future__ import annotations

import pytest
from pydantic import BaseModel

from oigtl import (
    RegistryConflictError,
    lookup_message_class,
    pack_envelope,
    pack_header,
    register_message_type,
    registered_types,
    unpack_envelope,
    unregister_message_type,
)
from oigtl.runtime.envelope import Envelope


# ---------------------------------------------------------------------------
# Fixture: clean up any registration the test installs
# ---------------------------------------------------------------------------


@pytest.fixture
def managed_type_id():
    """Yield a type_id unique to this test, guaranteed clean on exit.

    Must fit within OpenIGTLink's 12-byte type_id field; longer
    strings get silently truncated on pack and fail to round-trip.
    """
    tid = "EXT_TEST"  # 8 chars — well within the 12-byte cap
    yield tid
    unregister_message_type(tid)


# ---------------------------------------------------------------------------
# A minimal extension class that satisfies the body-class contract
# ---------------------------------------------------------------------------


class _FakeBody(BaseModel):
    """Toy body: four little-endian bytes interpreted as an int."""

    value: int

    @classmethod
    def unpack(cls, body: bytes) -> "_FakeBody":
        if len(body) != 4:
            raise ValueError(f"expected 4 bytes, got {len(body)}")
        return cls(value=int.from_bytes(body, "little", signed=False))

    def pack(self) -> bytes:
        return int(self.value).to_bytes(4, "little", signed=False)


# ---------------------------------------------------------------------------
# Built-ins are visible through the public API
# ---------------------------------------------------------------------------


def test_built_in_types_are_registered():
    types = registered_types()
    # Spot-check a handful that must always be present.
    for expected in ("TRANSFORM", "STATUS", "IMAGE", "COMMAND",
                     "POSITION", "POLYDATA", "STRING"):
        assert expected in types


def test_lookup_returns_built_in_class():
    from oigtl.messages import Transform
    assert lookup_message_class("TRANSFORM") is Transform


def test_lookup_returns_none_for_unknown():
    assert lookup_message_class("TOTALLY_NOT_A_REAL_TYPE") is None


# ---------------------------------------------------------------------------
# Extension registration — the main event
# ---------------------------------------------------------------------------


def test_registered_extension_decodes_via_public_api(managed_type_id):
    register_message_type(managed_type_id, _FakeBody)

    wire = _wrap_body(managed_type_id, _FakeBody(value=0xCAFEBABE).pack())
    env = unpack_envelope(wire)

    assert isinstance(env.body, _FakeBody)
    assert env.body.value == 0xCAFEBABE
    assert env.header.type_id == managed_type_id


def test_registered_extension_round_trips(managed_type_id):
    register_message_type(managed_type_id, _FakeBody)

    wire = _wrap_body(managed_type_id, _FakeBody(value=42).pack())
    env = unpack_envelope(wire)

    assert pack_envelope(env) == wire


# ---------------------------------------------------------------------------
# Collision detection and override
# ---------------------------------------------------------------------------


def test_duplicate_registration_raises(managed_type_id):
    register_message_type(managed_type_id, _FakeBody)

    class _Other(BaseModel):
        @classmethod
        def unpack(cls, body: bytes):  # pragma: no cover — not called
            return cls()
        def pack(self) -> bytes:       # pragma: no cover
            return b""

    with pytest.raises(RegistryConflictError):
        register_message_type(managed_type_id, _Other)


def test_idempotent_on_same_class(managed_type_id):
    register_message_type(managed_type_id, _FakeBody)
    # Re-registering the same (type_id, cls) pair is a no-op; no raise.
    register_message_type(managed_type_id, _FakeBody)
    assert lookup_message_class(managed_type_id) is _FakeBody


def test_override_replaces_existing(managed_type_id):
    class _Other(BaseModel):
        @classmethod
        def unpack(cls, body: bytes) -> "_Other":
            return cls()
        def pack(self) -> bytes:
            return b""

    register_message_type(managed_type_id, _FakeBody)
    register_message_type(managed_type_id, _Other, override=True)
    assert lookup_message_class(managed_type_id) is _Other


def test_built_in_collision_is_protected():
    """Can't silently clobber a built-in without override=True."""
    class _Imposter(BaseModel):
        @classmethod
        def unpack(cls, body: bytes) -> "_Imposter":
            return cls()
        def pack(self) -> bytes:
            return b""

    with pytest.raises(RegistryConflictError):
        register_message_type("TRANSFORM", _Imposter)


# ---------------------------------------------------------------------------
# unregister_message_type
# ---------------------------------------------------------------------------


def test_unregister_returns_prior_class(managed_type_id):
    register_message_type(managed_type_id, _FakeBody)
    prior = unregister_message_type(managed_type_id)
    assert prior is _FakeBody
    assert lookup_message_class(managed_type_id) is None


def test_unregister_missing_is_noop():
    assert unregister_message_type("NEVER_REGISTERED") is None


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _wrap_body(type_id: str, body: bytes) -> bytes:
    # version=1 — body is bare, no v2 extended-header region, so
    # pack_header's invariant would reject version>=2.
    header = pack_header(
        version=1,
        type_id=type_id,
        device_name="dev",
        timestamp=0,
        body=body,
    )
    return header + body
