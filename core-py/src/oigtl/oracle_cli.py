"""Persistent oracle subprocess for the differential fuzzer.

Same protocol as ``core-cpp/src/oracle_cli.cpp`` and
``core-ts/src/oracle_cli.ts``:

- stdin: one hex-encoded wire message per line (or empty line for
  a 0-byte input).
- stdout: one JSON oracle report per line, matching the stable
  cross-language shape (``ok``, ``type_id``, ``device_name``,
  ``version``, ``body_size``, ``ext_header_size``, ``metadata_count``,
  ``round_trip_ok``, ``error``).

Runs the TYPED oracle (``oigtl.runtime.oracle.typed_verify_wire_bytes``),
so it exercises the generated message classes and the numpy /
array.array coercion paths. Toggle the stdlib fallback with
``OIGTL_NO_NUMPY=1`` in the environment before starting this
process.

Invoke via:

    python -m oigtl.oracle_cli
"""

from __future__ import annotations

import json
import sys

from oigtl.runtime.oracle import typed_verify_wire_bytes


def _report_to_dict(result) -> dict:
    header = result.header
    ext = result.extended_header
    return {
        "ok": result.ok,
        "type_id": header.type_id if header else "",
        "device_name": header.device_name if header else "",
        "version": header.version if header else 0,
        "body_size": header.body_size if header else 0,
        "ext_header_size": ext.ext_header_size if ext else None,
        "metadata_count": len(result.metadata),
        "round_trip_ok": result.round_trip_ok,
        "error": result.error,
    }


def _emit_invalid_hex() -> None:
    sys.stdout.write(json.dumps({
        "ok": False,
        "type_id": "",
        "device_name": "",
        "version": 0,
        "body_size": 0,
        "ext_header_size": None,
        "metadata_count": 0,
        "round_trip_ok": False,
        "error": "oracle_cli: invalid hex input",
    }) + "\n")


def main() -> int:
    for line in sys.stdin:
        line = line.rstrip("\r\n")
        try:
            wire = bytes.fromhex(line)
        except ValueError:
            _emit_invalid_hex()
            sys.stdout.flush()
            continue
        try:
            result = typed_verify_wire_bytes(wire)
            sys.stdout.write(json.dumps(_report_to_dict(result)) + "\n")
        except Exception as exc:
            # Any uncaught exception is itself a bug — surface it as
            # a special error so the fuzzer records a disagreement
            # instead of crashing the oracle process.
            sys.stdout.write(json.dumps({
                "ok": False,
                "type_id": "",
                "device_name": "",
                "version": 0,
                "body_size": 0,
                "ext_header_size": None,
                "metadata_count": 0,
                "round_trip_ok": False,
                "error": f"oracle_cli: uncaught {type(exc).__name__}: {exc}",
            }) + "\n")
        sys.stdout.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main())
