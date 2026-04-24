"""Smoke tests for the c-upstream differential verifier.

The verifier CLI lives at
`security/verifiers/c-upstream/verifier_cli.c` and is built as
`oigtl_c_upstream_verifier_cli` from `core-cpp`. Tests skip
cleanly when the binary isn't present (local build without
`cmake --build core-cpp/build --target
oigtl_c_upstream_verifier_cli`). CI builds it as part of the
core-cpp matrix.

Terminology: this is a verifier — an independent
differential-check participant — not an authoritative oracle.
The oracle is py-ref. See security/verifiers/c-upstream/README.md.
"""

from __future__ import annotations

import json
import subprocess
from pathlib import Path

import pytest

from oigtl_corpus_tools.codec.message import pack_message

REPO_ROOT = Path(__file__).resolve().parents[2]
BINARY = REPO_ROOT / "core-cpp" / "build" / "oigtl_c_upstream_verifier_cli"


def _run_verifier(wire: bytes) -> dict:
    """Run one input through the verifier CLI, return the report."""
    proc = subprocess.run(
        [str(BINARY)],
        input=wire.hex() + "\n",
        capture_output=True,
        text=True,
        timeout=10,
    )
    assert proc.returncode == 0, proc.stderr
    return json.loads(proc.stdout.strip())


@pytest.mark.skipif(
    not BINARY.is_file(),
    reason=(
        "oigtl_c_upstream_verifier_cli not built; run "
        "`cmake --build core-cpp/build --target "
        "oigtl_c_upstream_verifier_cli` from repo root"
    ),
)
class TestCUpstreamVerifier:
    """End-to-end tests against spec-conformant inputs.

    Every input used here is known-good — canonical TRANSFORM or
    STATUS byte sequences the verifier must accept. Intentional
    divergences from our codecs (ASCII-permissiveness etc.) are
    out of scope for these tests; they're documented in
    security/verifiers/c-upstream/README.md.
    """

    def test_transform_v1_matches_py_ref(self):
        """Canonical TRANSFORM v1 — the verifier accepts and
        reports header fields matching what py-ref produced."""
        wire = pack_message(
            "TRANSFORM",
            "TestTracker",
            {"matrix": [1.0, 0.0, 0.0,
                        0.0, 1.0, 0.0,
                        0.0, 0.0, 1.0,
                        10.0, 20.0, 30.0]},
            version=1,
            timestamp=0x12345678,
        )
        report = _run_verifier(wire)
        assert report["ok"] is True
        assert report["type_id"] == "TRANSFORM"
        assert report["device_name"] == "TestTracker"
        assert report["version"] == 1
        assert report["body_size"] == 48
        assert report["round_trip_ok"] is True
        assert report["error"] == ""

    def test_status_v1_minimal(self):
        """STATUS with a short ASCII error_name and empty message."""
        wire = pack_message(
            "STATUS",
            "TestDevice",
            {
                "code": 1,        # OK
                "subcode": 0,
                "error_name": "OK",
                "status_message": "",
            },
            version=1,
            timestamp=0x12345678,
        )
        report = _run_verifier(wire)
        assert report["ok"] is True, report["error"]
        assert report["type_id"] == "STATUS"
        assert report["device_name"] == "TestDevice"
        assert report["version"] == 1
        assert report["round_trip_ok"] is True

    def test_v2_declines_out_of_scope(self):
        """v2 framing is out of scope — verifier declines with
        a descriptive error, not a crash."""
        wire = pack_message(
            "TRANSFORM",
            "v2Device",
            {"matrix": [1.0, 0.0, 0.0, 0.0, 1.0, 0.0,
                        0.0, 0.0, 1.0, 0.0, 0.0, 0.0]},
            version=2,
            timestamp=0,
        )
        report = _run_verifier(wire)
        assert report["ok"] is False
        assert "out of scope" in report["error"]
        # Header fields should still be parsed.
        assert report["type_id"] == "TRANSFORM"
        assert report["version"] == 2

    def test_unsupported_type_declines(self):
        """A message type the verifier's switch doesn't cover
        emits an unsupported-type report, not a crash. POSITION
        is a valid v1 type upstream supports but this MVP
        verifier does not; header parse succeeds, body dispatch
        declines."""
        wire = pack_message(
            "POSITION",
            "PosDev",
            {"position": [1.0, 2.0, 3.0],
             "quaternion": [0.0, 0.0, 0.0, 1.0]},
            version=1,
            timestamp=0,
        )
        report = _run_verifier(wire)
        assert report["ok"] is False
        assert "unsupported type_id" in report["error"]
        assert report["type_id"] == "POSITION"

    def test_truncated_input(self):
        """Less than 58 bytes: verifier rejects with a clear error."""
        short = b"\x00" * 30
        report = _run_verifier(short)
        assert report["ok"] is False
        assert "truncated" in report["error"]

    def test_bad_hex_input(self):
        """Odd-length hex string — verifier rejects without crashing."""
        proc = subprocess.run(
            [str(BINARY)],
            input="abc\n",  # odd number of hex chars
            capture_output=True,
            text=True,
            timeout=10,
        )
        assert proc.returncode == 0
        report = json.loads(proc.stdout.strip())
        assert report["ok"] is False
        assert report["error"] == "bad hex input"
