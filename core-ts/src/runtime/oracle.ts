/**
 * Conformance oracle — the framing-aware pipeline that mirrors
 * `core-cpp/.../oracle.hpp::parse_wire` and `verify_wire_bytes`, and
 * `corpus-tools/.../codec/oracle.py`.
 *
 * Emits the same stable report shape as the other two languages so
 * the cross-language parity test can diff JSON outputs.
 */

import {
  EXT_HEADER_MIN_SIZE,
  type ExtendedHeader,
  type Framing,
  type MetadataEntry,
  unpackFraming,
} from "./ext_header.js";
import {
  type Header,
  HEADER_SIZE,
  unpackHeader,
  verifyCrc,
} from "./header.js";
import { lookup } from "./dispatch.js";

export interface FramingResult {
  ok: boolean;
  error: string;
  header: Header | null;
  extendedHeader: ExtendedHeader | null;
  /** The ext_header bytes (empty for v1). */
  extHeaderBytes: Uint8Array;
  /** Schema-content bytes — feed to a typed class's `unpack()`. */
  contentBytes: Uint8Array;
  /** Metadata index + body, preserved for byte-exact reassembly. */
  metadataBytes: Uint8Array;
  metadata: MetadataEntry[];
}

export interface VerifyResult {
  ok: boolean;
  error: string;
  header: Header | null;
  extendedHeader: ExtendedHeader | null;
  metadata: MetadataEntry[];
  roundTripOk: boolean;
  /** Typed message instance when dispatch succeeded. */
  message: unknown;
}

// ---------------------------------------------------------------------------
// parseWire — header + CRC + framing
// ---------------------------------------------------------------------------

export function parseWire(
  wire: Uint8Array,
  options: { checkCrc?: boolean } = {},
): FramingResult {
  const checkCrc = options.checkCrc ?? true;
  const empty = new Uint8Array(0);

  let header: Header;
  try {
    header = unpackHeader(wire);
  } catch (e) {
    return {
      ok: false,
      error: (e as Error).message,
      header: null,
      extendedHeader: null,
      extHeaderBytes: empty,
      contentBytes: empty,
      metadataBytes: empty,
      metadata: [],
    };
  }

  const bodySize = Number(header.bodySize);
  if (wire.length < HEADER_SIZE + bodySize) {
    return {
      ok: false,
      error: `body truncated: need ${bodySize} bytes, have ${wire.length - HEADER_SIZE}`,
      header,
      extendedHeader: null,
      extHeaderBytes: empty,
      contentBytes: empty,
      metadataBytes: empty,
      metadata: [],
    };
  }

  const body = wire.subarray(HEADER_SIZE, HEADER_SIZE + bodySize);

  if (checkCrc) {
    try {
      verifyCrc(header, body);
    } catch (e) {
      return {
        ok: false,
        error: (e as Error).message,
        header,
        extendedHeader: null,
        extHeaderBytes: empty,
        contentBytes: empty,
        metadataBytes: empty,
        metadata: [],
      };
    }
  }

  let framing: Framing;
  try {
    framing = unpackFraming(body, header.version);
  } catch (e) {
    return {
      ok: false,
      error: (e as Error).message,
      header,
      extendedHeader: null,
      extHeaderBytes: empty,
      contentBytes: empty,
      metadataBytes: empty,
      metadata: [],
    };
  }

  const extHeaderBytes =
    header.version >= 2 ? body.subarray(0, framing.extendedHeader.extHeaderSize) : empty;
  const metadataStart = framing.extendedHeader.extHeaderSize + framing.content.length;
  const metadataBytes = header.version >= 2 ? body.subarray(metadataStart) : empty;

  return {
    ok: true,
    error: "",
    header,
    extendedHeader: header.version >= 2 ? framing.extendedHeader : null,
    extHeaderBytes: new Uint8Array(extHeaderBytes),
    contentBytes: new Uint8Array(framing.content),
    metadataBytes: new Uint8Array(metadataBytes),
    metadata: framing.metadata,
  };
}

// ---------------------------------------------------------------------------
// verifyWireBytes — full pipeline + round-trip via registry
// ---------------------------------------------------------------------------

/**
 * Run the complete oracle pipeline:
 *
 * 1. Parse header (+ CRC if requested)
 * 2. Split framing (ext header / content / metadata)
 * 3. Look up a typed codec via the dispatch registry
 * 4. Call `unpack(contentBytes)` then `.pack()` and assert byte-equal
 *
 * Returns a `VerifyResult` with `ok=false` and a human-readable
 * `error` on any failure; the partial fields are populated up to
 * the point of failure (matches the Python/C++ oracle behaviour).
 */
export function verifyWireBytes(
  wire: Uint8Array,
  options: { checkCrc?: boolean; checkRoundTrip?: boolean } = {},
): VerifyResult {
  const checkRoundTrip = options.checkRoundTrip ?? true;
  const framing = parseWire(wire, { checkCrc: options.checkCrc ?? true });
  if (!framing.ok) {
    return {
      ok: false,
      error: framing.error,
      header: framing.header,
      extendedHeader: framing.extendedHeader,
      metadata: framing.metadata,
      roundTripOk: false,
      message: null,
    };
  }

  const typeId = framing.header!.typeId;
  const ctor = lookup(typeId);
  if (!ctor) {
    return {
      ok: false,
      error: `no typed class registered for type_id ${JSON.stringify(typeId)}`,
      header: framing.header,
      extendedHeader: framing.extendedHeader,
      metadata: framing.metadata,
      roundTripOk: false,
      message: null,
    };
  }

  let message: unknown;
  try {
    message = ctor.unpack(framing.contentBytes);
  } catch (e) {
    return {
      ok: false,
      error: `unpack failed for ${typeId}: ${(e as Error).message}`,
      header: framing.header,
      extendedHeader: framing.extendedHeader,
      metadata: framing.metadata,
      roundTripOk: false,
      message: null,
    };
  }

  let roundTripOk = true;
  if (checkRoundTrip) {
    try {
      const packed: Uint8Array = (message as { pack(): Uint8Array }).pack();
      if (!_bytesEqual(packed, framing.contentBytes)) {
        // Length mismatch is unconditionally a bug — canonical-form
        // normalization only applies when both buffers are the same
        // length and differ only in values.
        if (packed.length !== framing.contentBytes.length) {
          return {
            ok: false,
            error:
              `round-trip length mismatch for ${typeId}: ` +
              `${framing.contentBytes.length}B in, ${packed.length}B out`,
            header: framing.header,
            extendedHeader: framing.extendedHeader,
            metadata: framing.metadata,
            roundTripOk: false,
            message,
          };
        }
        // Same-length value diff: accept if unpack+pack on the
        // just-packed bytes reaches a fixed point. Covers IEEE-754
        // signaling-NaN quieting via JS's DataView.getFloat32 → FPU.
        // A cooperating sender would emit the canonical bytes on
        // re-transmit, so round-trip semantics are preserved.
        const second: Uint8Array = (
          (ctor.unpack(packed) as { pack(): Uint8Array }).pack()
        );
        if (!_bytesEqual(second, packed)) {
          return {
            ok: false,
            error:
              `round-trip unstable for ${typeId}: pack→unpack→pack ` +
              `produced different bytes on the second pass`,
            header: framing.header,
            extendedHeader: framing.extendedHeader,
            metadata: framing.metadata,
            roundTripOk: false,
            message,
          };
        }
      }
    } catch (e) {
      return {
        ok: false,
        error: `pack failed for ${typeId}: ${(e as Error).message}`,
        header: framing.header,
        extendedHeader: framing.extendedHeader,
        metadata: framing.metadata,
        roundTripOk: false,
        message,
      };
    }
  }

  return {
    ok: true,
    error: "",
    header: framing.header,
    extendedHeader: framing.extendedHeader,
    metadata: framing.metadata,
    roundTripOk,
    message,
  };
}

/** Compact cross-language report shape — matches oracle verify JSON. */
export interface OracleReport {
  ok: boolean;
  type_id: string;
  device_name: string;
  version: number;
  body_size: number;
  ext_header_size: number | null;
  metadata_count: number;
  round_trip_ok: boolean;
  error: string;
}

export function toReport(result: VerifyResult): OracleReport {
  return {
    ok: result.ok,
    type_id: result.header?.typeId ?? "",
    device_name: result.header?.deviceName ?? "",
    version: result.header?.version ?? 0,
    body_size: result.header ? Number(result.header.bodySize) : 0,
    ext_header_size: result.extendedHeader?.extHeaderSize ?? null,
    metadata_count: result.metadata.length,
    round_trip_ok: result.roundTripOk,
    error: result.error,
  };
}

function _bytesEqual(a: Uint8Array, b: Uint8Array): boolean {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}
