/**
 * v2/v3 extended header + metadata region codec.
 *
 * Layout of the extended header (first 12 bytes of body):
 *
 *     off  size  field
 *       0     2  ext_header_size       (uint16, BE)
 *       2     2  metadata_header_size  (uint16, BE)
 *       4     4  metadata_size         (uint32, BE)
 *       8     4  message_id            (uint32, BE)
 *     total: 12 (declared by ext_header_size; may be larger
 *                on forward-compatible extensions — skip to it)
 *
 * Full v3 body layout:
 *
 *     [ extended_header | content_bytes | metadata_index | metadata_body ]
 *       ext_header_size    (body - eh - mh - ms)  mh_size     ms_size
 *
 * Metadata (when present):
 *
 *   index[N]:  [ key_size: u16 | value_encoding: u16 | value_size: u32 ]
 *   body:      concatenated utf-8 keys + value bytes in index order
 *   The first 2 bytes of the index region are an N (uint16) count.
 */

import { readU16, readU32, viewOf, writeU16, writeU32 } from "./byte_order.js";
import { BodyDecodeError } from "./errors.js";

export const EXT_HEADER_MIN_SIZE = 12;

export interface ExtendedHeader {
  extHeaderSize: number;
  metadataHeaderSize: number;
  metadataSize: number;
  messageId: number;
}

export interface MetadataEntry {
  key: string;
  /** Encoding code per IANA MIBenum (3 = US-ASCII, 106 = UTF-8). */
  encoding: number;
  /** Raw value bytes — decoding is the caller's responsibility. */
  value: Uint8Array;
}

export interface Framing {
  extendedHeader: ExtendedHeader;
  content: Uint8Array;
  metadata: MetadataEntry[];
}

// ---------------------------------------------------------------------------
// Extended header
// ---------------------------------------------------------------------------

export function unpackExtendedHeader(body: Uint8Array): ExtendedHeader {
  if (body.length < EXT_HEADER_MIN_SIZE) {
    throw new BodyDecodeError(
      `extended header needs ${EXT_HEADER_MIN_SIZE} bytes, got ${body.length}`,
    );
  }
  const view = viewOf(body);
  const extHeaderSize = readU16(view, 0);
  const metadataHeaderSize = readU16(view, 2);
  const metadataSize = readU32(view, 4);
  const messageId = readU32(view, 8);
  if (extHeaderSize < EXT_HEADER_MIN_SIZE) {
    throw new BodyDecodeError(
      `ext_header_size ${extHeaderSize} < min ${EXT_HEADER_MIN_SIZE}`,
    );
  }
  if (extHeaderSize > body.length) {
    throw new BodyDecodeError(
      `ext_header_size ${extHeaderSize} exceeds body length ${body.length}`,
    );
  }
  return { extHeaderSize, metadataHeaderSize, metadataSize, messageId };
}

export function packExtendedHeader(eh: ExtendedHeader): Uint8Array {
  const out = new Uint8Array(EXT_HEADER_MIN_SIZE);
  const view = viewOf(out);
  writeU16(view, 0, eh.extHeaderSize);
  writeU16(view, 2, eh.metadataHeaderSize);
  writeU32(view, 4, eh.metadataSize);
  writeU32(view, 8, eh.messageId);
  return out;
}

// ---------------------------------------------------------------------------
// Full body framing (v3): split into content + metadata
// ---------------------------------------------------------------------------

/**
 * Given the full body (the bytes after the 58-byte outer header),
 * extract the framing components per protocol version.
 *
 * For v1 bodies: `content = body` and metadata is empty.
 * For v2/v3 bodies: parses the 12-byte extended header, carves the
 * content region, and decodes the metadata key/value list if present.
 */
export function unpackFraming(body: Uint8Array, version: number): Framing {
  if (version < 2) {
    return {
      extendedHeader: {
        extHeaderSize: 0,
        metadataHeaderSize: 0,
        metadataSize: 0,
        messageId: 0,
      },
      content: body,
      metadata: [],
    };
  }
  const eh = unpackExtendedHeader(body);
  const contentStart = eh.extHeaderSize;
  const metadataStart = body.length - eh.metadataHeaderSize - eh.metadataSize;
  if (metadataStart < contentStart) {
    throw new BodyDecodeError(
      `framing inconsistent: content_end ${metadataStart} < content_start ${contentStart}`,
    );
  }
  const content = body.subarray(contentStart, metadataStart);
  const metadata = _unpackMetadata(
    body.subarray(metadataStart, metadataStart + eh.metadataHeaderSize),
    body.subarray(metadataStart + eh.metadataHeaderSize, body.length),
  );
  return { extendedHeader: eh, content, metadata };
}

function _unpackMetadata(header: Uint8Array, payload: Uint8Array): MetadataEntry[] {
  if (header.length === 0) return [];
  if (header.length < 2) {
    throw new BodyDecodeError(`metadata index too short: ${header.length}`);
  }
  const view = viewOf(header);
  const count = readU16(view, 0);
  if (count === 0) return [];
  // Each index entry is 8 bytes: u16 key_size, u16 encoding, u32 value_size.
  const expectedHeaderLen = 2 + count * 8;
  if (header.length < expectedHeaderLen) {
    throw new BodyDecodeError(
      `metadata index truncated: expected ${expectedHeaderLen}, got ${header.length}`,
    );
  }
  const entries: Array<{ keySize: number; encoding: number; valueSize: number }> = [];
  for (let i = 0; i < count; i++) {
    const off = 2 + i * 8;
    entries.push({
      keySize: readU16(view, off),
      encoding: readU16(view, off + 2),
      valueSize: readU32(view, off + 4),
    });
  }
  const decoder = new TextDecoder("utf-8", { fatal: false });
  const out: MetadataEntry[] = [];
  let cursor = 0;
  for (const e of entries) {
    if (cursor + e.keySize + e.valueSize > payload.length) {
      throw new BodyDecodeError(
        `metadata body truncated at entry ${out.length}`,
      );
    }
    const keyBytes = payload.subarray(cursor, cursor + e.keySize);
    cursor += e.keySize;
    const valueBytes = payload.subarray(cursor, cursor + e.valueSize);
    cursor += e.valueSize;
    out.push({
      key: decoder.decode(keyBytes),
      encoding: e.encoding,
      value: new Uint8Array(valueBytes), // copy so callers can keep it across buffer reuse
    });
  }
  return out;
}
