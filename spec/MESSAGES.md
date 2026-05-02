# OpenIGTLink Message Reference

> **This file is generated** from `spec/schemas/*.json` by
> `oigtl-corpus messages-doc`. Do not edit by hand — re-run the
> generator and commit the output. CI verifies it stays in sync
> on every PR.

This is the reference for every message type and framing structure
defined by the OpenIGTLink protocol. For the protocol itself (wire
format, framing, header layout, CRC, transport), see
[`protocol/v3.md`](protocol/v3.md). For the schemas this file is
generated from, see [`schemas/`](schemas/).


## Index

### Data messages

Carry payloads of typed data on the wire. The bulk of OpenIGTLink traffic — pose, image, sensor, status, command.

- [`BIND`](#bind) — Multi-message container that bundles N child messages into a single wire message.
- [`CAPABILITY`](#capability) — Advertises the list of OpenIGTLink message types that a device accepts.
- [`COLORT`](#colort) — Color lookup table that maps integer pixel values to display colors.
- [`COLORTABLE`](#colortable) — Legacy wire alias for COLORTABLE.
- [`COMMAND`](#command) — Carries a structured command — typically an XML document — from one peer to another, tagged with a session-unique ID and a symbolic name so replies (RTS_COMMAND) can reference it.
- [`IMAGE`](#image) — Delivers image pixel data — 2D frames or 3D volumes — together with orientation, origin, scalar type, and optional partial-volume windowing.
- [`IMGMETA`](#imgmeta) — Advertises the set of IMAGE volumes available on a server.
- [`LBMETA`](#lbmeta) — Advertises the set of label-map regions available as IMAGE messages on a server.
- [`NDARRAY`](#ndarray) — Variable-rank N-dimensional numerical array.
- [`POINT`](#point) — List of 3D annotated points: landmarks, fiducials, surgical targets, and similar discrete spatial markers.
- [`POLYDATA`](#polydata) — Polygonal mesh — points, topology (vertices, lines, polygons, triangle strips), and per-point or per-cell attributes.
- [`POSITION`](#position) — 3D position with an optional orientation quaternion.
- [`QTDATA`](#qtdata) — Stream of tracked-tool poses using position + quaternion representation.
- [`QTRANS`](#qtrans) — Position + full quaternion orientation in 28 bytes fixed.
- [`SENSOR`](#sensor) — Reports a vector of scalar sensor readings with an attached SI unit-of-measure descriptor.
- [`STATUS`](#status) — Reports the operational status of a device or the outcome of a request.
- [`STRING`](#string) — Carries a character string with an explicit character-encoding hint.
- [`TDATA`](#tdata) — Stream of tracked-tool poses using a 3×4 transformation matrix per tool.
- [`TRAJ`](#traj) — List of 3D trajectories — planned or executed paths from an entry point to a target point.
- [`TRANSFORM`](#transform) — 4x4 homogeneous transformation matrix in a right-handed coordinate system.
- [`VIDEO`](#video) — Compressed video frame with in-band orientation.
- [`VIDEOMETA`](#videometa) — Advertises the set of VIDEO sources available on a server.

### Query messages — `GET_*`

Request a single snapshot of a data message type. The peer responds with the matching data message.

- [`GET_BIND`](#get-bind) — Request specific child messages from a BIND server.
- [`GET_CAPABIL`](#get-capabil) — Request a CAPABILITY listing from the remote peer.
- [`GET_COLORT`](#get-colort) — Request the COLORTABLE for the device named in the header.
- [`GET_IMAGE`](#get-image) — Request the current IMAGE for the device named in the header.
- [`GET_IMGMETA`](#get-imgmeta) — Request the IMGMETA listing.
- [`GET_LBMETA`](#get-lbmeta) — Request the LBMETA listing.
- [`GET_NDARRAY`](#get-ndarray) — Request the current NDARRAY for the device named in the header.
- [`GET_POINT`](#get-point) — Request the POINT set for the device named in the header.
- [`GET_POLYDATA`](#get-polydata) — Request the current POLYDATA mesh for the device named in the header.
- [`GET_POSITION`](#get-position) — Request the current POSITION for the device named in the header.
- [`GET_QTDATA`](#get-qtdata) — Request the current QTDATA for the device named in the header.
- [`GET_QTRANS`](#get-qtrans) — Request the current QTRANS for the device named in the header.
- [`GET_SENSOR`](#get-sensor) — Request the current SENSOR reading for the device named in the header.
- [`GET_STATUS`](#get-status) — Request the current STATUS for the device named in the header.
- [`GET_STRING`](#get-string) — Request the current STRING for the device named in the header.
- [`GET_TDATA`](#get-tdata) — Request the current TDATA for the device named in the header.
- [`GET_TRAJ`](#get-traj) — Request the TRAJECTORY set for the device named in the header.
- [`GET_TRANS`](#get-trans) — Request the current TRANSFORM for the device named in the header.
- [`GET_VMETA`](#get-vmeta) — Request the VIDEOMETA listing.

### Stream-start messages — `STT_*`

Open a streaming subscription to a data message type. The peer publishes data messages until told to stop.

- [`STT_BIND`](#stt-bind) — Start streaming BIND messages.
- [`STT_IMAGE`](#stt-image) — Start streaming IMAGE messages at the server's default rate.
- [`STT_NDARRAY`](#stt-ndarray) — Start streaming NDARRAY messages.
- [`STT_POLYDATA`](#stt-polydata) — Start streaming POLYDATA messages at the server's default rate.
- [`STT_POSITION`](#stt-position) — Start streaming POSITION messages at the server's default rate.
- [`STT_QTDATA`](#stt-qtdata) — Start streaming QTDATA (quaternion tracking data) messages at a specified update interval, in a named coordinate system.
- [`STT_QTRANS`](#stt-qtrans) — Start streaming QTRANS messages at the server's default rate.
- [`STT_TDATA`](#stt-tdata) — Start streaming TDATA (tracking data) messages at a specified update interval, in a named coordinate system.
- [`STT_TRANS`](#stt-trans) — Start streaming TRANSFORM messages at the server's default rate.
- [`STT_VIDEO`](#stt-video) — Start streaming VIDEO frames with a specified codec and update interval.

### Stream-stop messages — `STP_*`

Close a streaming subscription opened by a `STT_*` message.

- [`STP_BIND`](#stp-bind) — Stop streaming BIND messages previously started by STT_BIND.
- [`STP_IMAGE`](#stp-image) — Stop streaming IMAGE messages previously started by STT_IMAGE.
- [`STP_NDARRAY`](#stp-ndarray) — Stop streaming NDARRAY messages previously started by STT_NDARRAY.
- [`STP_POLYDATA`](#stp-polydata) — Stop streaming POLYDATA previously started by STT_POLYDATA.
- [`STP_POSITION`](#stp-position) — Stop streaming POSITION messages previously started by STT_POSITION.
- [`STP_QTDATA`](#stp-qtdata) — Stop streaming QTDATA messages previously started by STT_QTDATA.
- [`STP_QTRANS`](#stp-qtrans) — Stop streaming QTRANS messages previously started by STT_QTRANS.
- [`STP_SENSOR`](#stp-sensor) — Stop streaming SENSOR messages previously started by STT_SENSOR.
- [`STP_TDATA`](#stp-tdata) — Stop streaming TDATA messages previously started by STT_TDATA.
- [`STP_TRANS`](#stp-trans) — Stop streaming TRANSFORM messages previously started by STT_TRANS.
- [`STP_VIDEO`](#stp-video) — Stop streaming VIDEO frames previously started by STT_VIDEO.

### Response messages — `RTS_*`

Status replies to streaming-control commands. Carry an outcome code that the requester acts on.

- [`RTS_BIND`](#rts-bind) — Server's return status for a BIND query (GET/STT/STP).
- [`RTS_CAPABIL`](#rts-capabil) — Server's return status for a CAPABILITY query (GET/STT/STP).
- [`RTS_COMMAND`](#rts-command) — Reply to a COMMAND message.
- [`RTS_IMAGE`](#rts-image) — Server's return status for a IMAGE query (GET/STT/STP).
- [`RTS_IMGMETA`](#rts-imgmeta) — Server's return status for a IMGMETA query (GET/STT/STP).
- [`RTS_LBMETA`](#rts-lbmeta) — Server's return status for a LBMETA query (GET/STT/STP).
- [`RTS_NDARRAY`](#rts-ndarray) — Server's return status for a NDARRAY query (GET/STT/STP).
- [`RTS_POINT`](#rts-point) — Server's return status for a POINT query (GET/STT/STP).
- [`RTS_POLYDATA`](#rts-polydata) — Server's return status for a POLYDATA query (GET/STT/STP).
- [`RTS_POSITION`](#rts-position) — Server's return status for a POSITION query (GET/STT/STP).
- [`RTS_QTDATA`](#rts-qtdata) — Server's return status for a QTDATA query (GET/STT/STP).
- [`RTS_QTRANS`](#rts-qtrans) — Server's return status for a QTRANS query (GET/STT/STP).
- [`RTS_SENSOR`](#rts-sensor) — Server's return status for a SENSOR query (GET/STT/STP).
- [`RTS_STATUS`](#rts-status) — Server's return status for a STATUS query (GET/STT/STP).
- [`RTS_STRING`](#rts-string) — Server's return status for a STRING query (GET/STT/STP).
- [`RTS_TDATA`](#rts-tdata) — Server's return status for a TDATA query (GET/STT/STP).
- [`RTS_TRAJ`](#rts-traj) — Server's return status for a TRAJECTORY query (GET/STT/STP).
- [`RTS_TRANS`](#rts-trans) — Server's return status for a TRANSFORM query (GET/STT/STP).

### Framing structures

Wire-level structural records — not user-facing message types. Documented here for reference because their schemas are part of the spec.

- [`EXT_HEADER`](#ext-header) — The v3 extended header, located at the start of the message body.
- [`HEADER`](#header) — The 58-byte fixed header that precedes every OpenIGTLink message on the wire.
- [`METADATA`](#metadata) — The metadata block that carries arbitrary key/value pairs, present in v2 and v3 messages.
- [`UNIT`](#unit) — Physical unit encoding — a 8-byte (uint64) packed representation of an SI unit with a metric prefix.

---

## Data messages

### `BIND`

**Type ID:** `BIND` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Multi-message container that bundles N child messages into a single wire message. The body carries a header table (one entry per child, declaring its type_id and body_size), a name table (null-terminated device names, 2-byte aligned), and then the concatenated child bodies (each padded to even length). A receiver splits the trailing body section into per-child payloads by reading the sizes from the header table.

**Rationale:** BIND exists to atomically deliver a coherent set of messages that belong together — for example, an IMAGE plus its TRANSFORM plus a STATUS, all sharing a single timestamp and arriving as one unit. Without BIND, a receiver might see the IMAGE before the TRANSFORM, briefly rendering the frame at the wrong position. BIND trades the complexity of a container format for guaranteed atomicity on the wire.

**Fields:**

**`ncmessages`** &nbsp;·&nbsp; `uint16`

Number of child messages bundled in this BIND. Determines the length of the `header_entries` array and the number of entries in the name table. 0 is legal but degenerate (empty BIND).

**`header_entries`** &nbsp;·&nbsp; `struct × ncmessages`

Array of N × 20-byte header entries (type_id[12] + body_size[8]). A sequential reader consumes these first, then uses the body_size values to partition the trailing bodies section into per-child payloads.

*Element fields:*

- **`type_id`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 12 B — Wire type identifier of the i-th child message (e.g. 'IMAGE', 'TRANSFORM'). Same 12-byte null-padded format as the outer OpenIGTLink header's type field.
- **`body_size`** &nbsp;·&nbsp; `uint64` — Byte size of the i-th child message's body (not including the child's own OpenIGTLink header — it has none; the child is embedded raw). Used to find the boundary between child[i] and child[i+1] in the trailing bodies section.

**`nametable_size`** &nbsp;·&nbsp; `uint16`

Byte length of the name table section that immediately follows. MUST be even (2-byte aligned); a receiver MUST reject an odd value. The name table contains ncmessages null-terminated device-name strings packed together, with a padding byte appended if the total string bytes (including null terminators) is odd.

**`name_table`** &nbsp;·&nbsp; `uint8 × nametable_size`

Packed device names — ncmessages null-terminated ASCII strings concatenated together, 2-byte-aligned to nametable_size bytes. To extract name[i], walk forward from the start, reading each null-terminated string in sequence. Each name is at most 20 bytes (IGTL_HEADER_NAME_SIZE). An empty name (single null byte) is legal.

**`bodies`** &nbsp;·&nbsp; `uint8 × (remaining)`

Concatenated child message bodies. Child[i] occupies body_size[i] bytes (from header_entries[i]), followed by a 1-byte padding if body_size[i] is odd. A receiver MUST partition this blob using the header_entries array; without it the boundaries are unrecoverable. The total byte count MUST equal sum(ceil_to_even(header_entries[i].body_size) for i in 0..ncmessages-1). Any excess or shortfall is a malformed message.

**Post-unpack invariant:** `bind` — see `corpus-tools/src/oigtl_corpus_tools/codec/policy.py` for the cross-codec invariant definition.

**Legacy notes:**

- Total body_size = 2 (ncmessages) + 20*N (header_entries) + 2 (nametable_size) + nametable_size + sum(body_size[i] + body_size[i]%2 for each child). The padding rule (odd child bodies get +1 byte) matches VTK's word-alignment convention.
- Upstream igtl_bind_unpack_normal (at pinned SHA 94244fe) carries an explicit '/** TODO: check the total size of the message? **/' at line 189 — it does NOT validate body_size before iterating ncmessages entries. An attacker setting ncmessages=65535 with a tiny body causes massive OOB reads through the header table loop.
- Upstream igtl_bind_get_size_request (for GET_BIND) computes ntable_size at lines 644-648 but never adds it to the returned size (line 650). GET_BIND messages with non-empty name tables will have an incorrect size, causing CRC mismatches or buffer under-allocation.
- Name table parsing in igtl_bind_unpack_normal iterates N times using strlen() + ptr advance (lines 169-175) with no bounds check against nametable_size. An attacker can make the parser walk past the name table boundary into the bodies section or beyond — classic unbounded strlen on attacker-controlled data.
- The nametable_size field is uint16, limiting the name table to 65534 bytes (even). With N entries of up to 20 characters + null = 21 bytes each, this bounds N to roughly 3120 children before the name table overflows. Practical BIND messages rarely exceed single-digit children.
- Child bodies inside a BIND do NOT carry their own OpenIGTLink headers; they are raw body payloads. The type_id and body_size in the BIND header table serve the role of the child's outer header. A receiver that needs to dispatch the child payload to a type-specific unpacker must use the type_id from the header table, not from the child body itself.
- GET_BIND carries only (ncmessages, type_id[N], nametable_size, name_table) — no body_size per entry and no bodies section. STT_BIND prepends a uint64 time-resolution field before the GET_BIND layout. STP_BIND has an empty body. RTS_BIND is a single uint8 status byte. These variants are not separate schema files; they share the BIND wire type family.

**See also:** [`IMAGE`](#image), [`TRANSFORM`](#transform), [`STATUS`](#status)

**Spec reference:** [protocol/v3.md §"Body (BIND)"](protocol/v3.md)

---

### `CAPABILITY`

**Type ID:** `CAPABILITY` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Advertises the list of OpenIGTLink message types that a device accepts. Typically sent in response to a GET_CAPABIL query during connection setup; peers use the advertised list to decide which features to use on the session.

**Rationale:** A bare array of 12-byte type-id strings is the simplest and most forward-compatible way to express 'what this device speaks.' No leading count field is needed because the number of entries is implicit in the body size (body_size = 12 * N), and a body whose size is not a multiple of 12 is definitionally malformed.

**Fields:**

**`supported_types`** &nbsp;·&nbsp; `fixed_string<12> × (remaining)`

Ordered array of OpenIGTLink message-type identifiers. Each element is a 12-byte ASCII string, null-padded on the right, matching the 12-byte `type` field of the outer message header. The number of entries is derived at parse time as body_size / 12; a body whose size is not a multiple of 12 MUST be rejected.

**Legacy notes:**

- Total body size is exactly 12 * N where N is the number of supported types. An empty CAPABILITY body (N=0, body_size=0) is legal but unusual — it means 'no types supported'. Upstream C++ library (at pinned SHA 94244fe) treats an empty body as an error in CapabilityMessage::UnpackContent (igtlCapabilityMessage.cxx:133-136) and returns 0; a conformant implementation MAY accept or reject an empty list but MUST NOT crash.
- Partial-element bodies (body_size not a multiple of 12) are rejected by upstream via the explicit `pack_size % IGTL_HEADER_TYPE_SIZE > 0` check in igtl_capability_unpack (igtl_capability.c:108). Conformant implementations MUST preserve this rejection.
- Type identifiers inside the list follow the same 12-byte null-padded convention as the outer header's `type` field (see 'Message type strings' in protocol/v3.md). Duplicates in the list are not defined; receivers SHOULD treat the list as a set, not a multiset.

**See also:** [`STATUS`](#status), [`BIND`](#bind)

**Spec reference:** [protocol/v3.md §"Body (CAPABILITY)"](protocol/v3.md)

---

### `COLORT`

**Type ID:** `COLORT` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Color lookup table that maps integer pixel values to display colors. The body is a 2-byte header (index type + map type) followed by a raw byte array carrying the table entries. The table dimensions are fully determined by the two header fields: index type sets the number of entries (256 for uint8, 65536 for uint16) and map type sets the bytes per entry (1 for uint8, 2 for uint16, 3 for RGB).

**Rationale:** Label-map IMAGEs store integer pixel values, not colors. COLORTABLE ships the mapping from those integers to display colors — a standard technique in medical imaging (DICOM supplies equivalent functionality via LUT descriptors). A single COLORTABLE can serve multiple label maps as long as they share the same index range. Separating the LUT from the image keeps IMAGE messages compact for streaming and lets the LUT be updated independently.

**Fields:**

**`index_type`** &nbsp;·&nbsp; `int8`

Index type — determines the number of table entries: 3 = uint8 (256 entries), 5 = uint16 (65536 entries). These values match the scalar_type codes used elsewhere in the protocol. Receivers MUST reject any value other than 3 or 5.

**`map_type`** &nbsp;·&nbsp; `int8`

Map type — determines the bytes per entry: 3 = uint8 (1 byte per entry), 5 = uint16 (2 bytes per entry), 19 = RGB (3 bytes per entry: R, G, B in that order). Receivers MUST reject values not in {3, 5, 19}.

**`table`** &nbsp;·&nbsp; `uint8 × (remaining)`

Raw color table data — N × S bytes where N is the entry count from index_type (256 or 65536) and S is the bytes per entry from map_type (1, 2, or 3). The schema models this as raw bytes (uint8[]) because the per-entry width depends on map_type. Receivers MUST verify len(table) exactly equals N × S; a mismatch means the body_size is inconsistent with the header and the message MUST be rejected.

**Post-unpack invariant:** `colortable` — see `corpus-tools/src/oigtl_corpus_tools/codec/policy.py` for the cross-codec invariant definition.

**Legacy notes:**

- Expected body_size = 2 + N × S where N ∈ {256, 65536} and S ∈ {1, 2, 3}. The six valid table sizes are: 256 (uint8/uint8), 512 (uint8/uint16), 768 (uint8/RGB), 65536 (uint16/uint8), 131072 (uint16/uint16), 196608 (uint16/RGB). Maximum body is 196610 bytes (~192 KiB) — comfortably bounded.
- The wire type_id is 'COLORT' (6 chars), not 'COLORTABLE'; the 12-byte type field null-pads the remainder. The logical message_type name in this schema uses the full 'COLORTABLE' for clarity.
- index_type and map_type are int8 on the wire (signed), matching the upstream C struct. The actual values (3, 5, 19) are all positive and fit in either int8 or uint8; the signed-ness has no practical effect but is preserved for wire fidelity.
- Upstream igtl_colortable_convert_byte_order only byte-swaps when mapType is uint16 (value 5), which is correct: uint8 and RGB entries are single-byte values that don't need swapping. A conformant implementation MUST match this behavior.
- Upstream igtl_colortable_get_table_size uses a fallthrough pattern: any index_type other than 3 is treated as uint16 (65536 entries), and any map_type other than 3 or 5 is treated as RGB (3 bytes). A conformant implementation MUST NOT rely on this fallthrough and SHOULD reject unknown values explicitly.
- Upstream C library does NOT validate body_size against the expected table_size before accessing the table region. A conformant implementation MUST verify body_size == 2 + expected_table_bytes before any table access.

**See also:** [`IMAGE`](#image), [`LBMETA`](#lbmeta), [`IMGMETA`](#imgmeta), [`VIDEOMETA`](#videometa)

**Spec reference:** [protocol/v3.md §"Body (COLORTABLE)"](protocol/v3.md)

---

### `COLORTABLE`

**Type ID:** `COLORTABLE` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Legacy wire alias for COLORTABLE. Body layout is identical to the modern `COLORT` type_id; the only difference is the 10-character wire string that predates upstream shortening to 'COLORT'. Kept as a distinct schema so receivers can round-trip pre-shortening traffic without ambiguity.

**Rationale:** The upstream C++ library at some point shortened its `m_SendMessageType` from 'COLORTABLE' to 'COLORT'. Fixtures captured before the change still contain the 10-byte name in the 12-byte type field. New senders MUST use 'COLORT' (see colortable.json); receivers that care about backward compatibility with older deployments SHOULD accept this legacy name too.

**Fields:**

**`index_type`** &nbsp;·&nbsp; `int8`

Index type — 3 = uint8 (256 entries), 5 = uint16 (65536 entries). Same semantics as the modern COLORT schema.

**`map_type`** &nbsp;·&nbsp; `int8`

Map type — 3 = uint8 (1 byte/entry), 5 = uint16 (2 bytes/entry), 19 = RGB (3 bytes/entry).

**`table`** &nbsp;·&nbsp; `uint8 × (remaining)`

Raw color table data. Same rules as COLORT.

**Post-unpack invariant:** `colortable` — see `corpus-tools/src/oigtl_corpus_tools/codec/policy.py` for the cross-codec invariant definition.

**Legacy notes:**

- This schema exists solely to let receivers decode pre-shortening wire traffic that uses 'COLORTABLE' as the type_id. New senders MUST use the 'COLORT' type_id (see colortable.json).
- The body layout is byte-identical to the modern COLORT schema — only the 12-byte type_id field differs on the wire.

**See also:** [`COLORTABLE`](#colortable), [`IMAGE`](#image), [`LBMETA`](#lbmeta)

**Spec reference:** [protocol/v3.md §"Body (COLORTABLE)"](protocol/v3.md)

---

### `COMMAND`

**Type ID:** `COMMAND` &nbsp;·&nbsp; **Introduced in:** v3 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Carries a structured command — typically an XML document — from one peer to another, tagged with a session-unique ID and a symbolic name so replies (RTS_COMMAND) can reference it. The body is a 138-byte fixed header followed by a variable-length command string whose byte count is declared in the header's `length` field and whose character encoding is declared by the `encoding` field (an IANA MIBenum value).

**Rationale:** STRING already lets a sender ship an arbitrary text blob, but in real deployments a command channel needs two additional pieces: a stable ID so the sender can correlate a reply with its request, and a symbolic name so the receiver can dispatch without parsing the payload. COMMAND adds both with a dense 138-byte header, and it extends STRING's uint16 length field to uint32 so a command payload can exceed 64 KiB — relevant for DICOM-adjacent or image-manipulation commands. The `encoding` field is carried explicitly rather than hard-coded to ASCII so a deployment using Latin-1 or UTF-8 (MIBenum 106) can be unambiguous on the wire; US-ASCII (MIBenum 3) is the default and the strongly-recommended choice for interoperability.

**Fields:**

**`command_id`** &nbsp;·&nbsp; `uint32`

Session-unique identifier chosen by the sender. A receiver echoes this value in any RTS_COMMAND reply so the sender can correlate responses with outstanding requests. Uniqueness and wraparound policy are the sender's responsibility; the protocol imposes no requirement beyond 'unique within a connection's lifetime'.

**`command_name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 128 B

Symbolic command name, null-padded to 128 bytes. Used by the receiver to dispatch without parsing the command payload. Sender SHOULD choose a name that is stable across versions of the same command. An empty name (all null bytes) is legal but discouraged.

**`encoding`** &nbsp;·&nbsp; `uint16`

IANA MIBenum code for the character encoding of the `command` bytes (e.g. 3 = US-ASCII, 106 = UTF-8, 4 = ISO-8859-1). See https://www.iana.org/assignments/character-sets. Receivers MUST reject values not in the IANA table and SHOULD treat 3 (US-ASCII) as the interoperable default when not otherwise constrained.

**`length`** &nbsp;·&nbsp; `uint32`

Byte length of the trailing `command` field. Receivers MUST verify `body_size >= 138 + length` before any access to the command region; upstream C++ does not (see U-9). When length is zero, the `command` field occupies no body bytes and the message carries only the symbolic name / ID.

**`command`** &nbsp;·&nbsp; `uint8 × length`

Command payload — exactly `length` bytes, interpreted per `encoding`. Typically XML, but the protocol treats the bytes as opaque and applies no structure check. The payload MUST NOT be null-terminated on the wire (a trailing null would be included in `length`); receivers that need a C string SHOULD copy and null-terminate into a fresh buffer.

**Legacy notes:**

- COMMAND was introduced in protocol v3.0 (January 2017). Earlier deployments that need a command channel use STRING with an application-layer convention; COMMAND is the protocol's standardized replacement.
- The fixed header is exactly 138 bytes: 4 (command_id) + 128 (command_name) + 2 (encoding) + 4 (length) = 138. Total body_size on the wire is 138 + length (plus any trailing metadata block the framework appends).
- `encoding` is carried as a uint16, wide enough for the full IANA MIBenum range. The upstream C++ library validates incoming encoding on the sender's SetContentEncoding setter against a hand-maintained list of 257 values, but `UnpackContent` does NOT validate the encoding field on receive. Conformant implementations SHOULD validate on both sides.
- The upstream C++ setter `CommandMessage::SetCommandContent` enforces a 16-bit cap on content length (`if (strlen(string) > 0xFFFF) return 0`), tighter than the 32-bit wire format. A conformant reimplementation MAY choose to honor the full uint32 range, but interoperating with upstream senders means practical payloads are bounded by 65535 bytes.
- Related message type RTS_COMMAND is a reply carrying the same ID and name, plus an error string reusing the same 128-byte buffer. Not a distinct wire schema — it reuses the COMMAND body layout with the understanding that `command_name` is repurposed as the error name on the reply direction.
- Upstream C++ `CommandMessage::UnpackContent` does NOT validate body_size against 138 or against 138 + length before accessing the header or appending the command region (see U-9). A conformant implementation MUST reject any COMMAND message where `body_size < 138` or `body_size < 138 + length` before any field access.
- Upstream C++ `CommandMessage::SetCommandName` and `RTSCommandMessage::SetCommandErrorString` use `strlen(x) > IGTL_COMMAND_NAME_SIZE` then `strcpy`, an off-by-one that permits a 128-character name to overflow the 128-byte buffer by one null byte (see U-10). This is a sender-side bug and does not affect wire interpretation, but a conformant implementation MUST use `>=` or an explicit bounded copy.

**See also:** [`STRING`](#string), [`STATUS`](#status)

**Spec reference:** [protocol/v3.md §"Body (COMMAND)"](protocol/v3.md)

---

### `IMAGE`

**Type ID:** `IMAGE` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Delivers image pixel data — 2D frames or 3D volumes — together with orientation, origin, scalar type, and optional partial-volume windowing. The body is a 72-byte image header followed by a byte array of pixel data. Supports streaming a full volume in one message or splitting it across multiple messages via the subvolume offset/size window.

**Rationale:** IMAGE is the workhorse of OpenIGTLink traffic for imaging modalities (CT, MR, US). The fixed 72-byte header carries everything a receiver needs to interpret the trailing pixel bytes — component count, scalar type, byte order, coordinate convention, extent, and a 3x4 orientation matrix — plus an optional subvolume window that lets a sender stream slices or tiles of a larger volume through a sequence of IMAGE messages. Putting orientation and extent in-band means the receiver can scale, rotate, and place each frame in 3D scene space without waiting for an accompanying TRANSFORM.

**Fields:**

**`header_version`** &nbsp;·&nbsp; `uint16`

IMAGE header format version, currently 1. This is the per-message-type format version, not the OpenIGTLink protocol version. Receivers MUST reject headers whose `header_version` they do not implement; upstream C++ UnpackContent returns 0 (failure) on mismatch.

**`num_components`** &nbsp;·&nbsp; `uint8`

Number of scalar components per pixel (e.g. 1 for grayscale, 3 for RGB, 3 for vector displacement fields). Must be > 0. Senders typically set 1 (scalar) or 3 (vector).

**`scalar_type`** &nbsp;·&nbsp; `uint8`

Pixel scalar type code: 2=int8, 3=uint8, 4=int16, 5=uint16, 6=int32, 7=uint32, 10=float32, 11=float64. Receivers MUST reject unknown values rather than silently treating them as 0-byte scalars — the latter is how upstream C `igtl_image_get_data_size` silently under-reports payload size for float types (see U-7).

**`endian`** &nbsp;·&nbsp; `uint8`

Byte order of the pixel data only (not the header, which is always big-endian / network order): 1=big, 2=little. Allows a sender to ship already-native-layout pixels to a known-matching receiver without swapping, at the cost of the receiver having to detect and conditionally swap.

**`coord`** &nbsp;·&nbsp; `uint8`

Coordinate-system convention for the matrix and origin: 1=RAS (right/anterior/superior, Slicer-style), 2=LPS (left/posterior/superior, DICOM-style). Receivers MUST accept both and convert as needed; unknown values SHOULD be treated as RAS for forward compatibility.

**`size`** &nbsp;·&nbsp; `uint16 × 3`

Entire volume extent in voxels: (Sx, Sy, Sz). For a 2D image, Sz=1. This is the total logical volume; the actual pixels carried in this message are bounded by `subvol_offset` + `subvol_size`.

**`matrix`** &nbsp;·&nbsp; `float32 × 12`

Orientation and origin, laid out as four float32[3] groups in row order: matrix[0..2] = norm_i * pixel_size_i, matrix[3..5] = norm_j * pixel_size_j, matrix[6..8] = norm_k * pixel_size_k, matrix[9..11] = origin (coordinates of the volume's reference point in millimeters, per `coord` convention). The matrix fills the upper 3x4 of a 4x4 homogeneous transform; the bottom row is always (0, 0, 0, 1) and is not transmitted.

**`subvol_offset`** &nbsp;·&nbsp; `uint16 × 3`

Starting voxel of the sub-region carried by this message within the full volume: (Ox, Oy, Oz). (0, 0, 0) means 'start at the origin voxel'. For a full-volume transfer, offset is (0,0,0) and subvol_size equals size.

**`subvol_size`** &nbsp;·&nbsp; `uint16 × 3`

Extent of the sub-region carried by this message: (Wx, Wy, Wz). The trailing `pixels` array MUST contain exactly num_components * Wx * Wy * Wz * sizeof(scalar_type) bytes. Receivers MUST verify subvol_offset + subvol_size <= size (componentwise) and MUST reject the message on overflow; unchecked arithmetic on attacker-controlled subvol_size caused the legacy library to miscompute payload length in 32-bit intermediate form (see U-8).

**`pixels`** &nbsp;·&nbsp; `uint8 × (remaining)`

Raw pixel payload — num_components * subvol_size[0] * subvol_size[1] * subvol_size[2] scalars of `scalar_type`, laid out in i-fastest order, in the byte order declared by `endian`. Receivers MUST verify len(pixels) equals the product formula above (in 64-bit arithmetic); any deviation is a malformed message and MUST be rejected. When splitting a volume across messages, senders SHOULD keep each message under ~1 MiB body so receivers can buffer safely.

**Post-unpack invariant:** `image` — see `corpus-tools/src/oigtl_corpus_tools/codec/policy.py` for the cross-codec invariant definition.

**Legacy notes:**

- The 72-byte fixed header is followed immediately by pixel bytes — there is no separator, no length prefix, and no trailing padding. The sender's responsibility is to set body_size (in the outer OpenIGTLink header) to exactly 72 + num_components * prod(subvol_size) * sizeof(scalar_type).
- Upstream C++ `ImageMessage::UnpackContent` does NOT validate body_size against `IGTL_IMAGE_HEADER_SIZE` before casting m_Content to igtl_image_header*. A body smaller than 72 bytes causes a 72-(body_size) byte OOB read in the byte-swap pass. A conformant implementation MUST reject any IMAGE message whose body_size < 72, before any header field access.
- Upstream C++ `ImageMessage::UnpackContent` does NOT validate that body_size >= 72 + expected_pixel_bytes before exposing m_Image to the caller. A malicious sender with body_size = 72 but subvol_size claiming 1GiB of pixels leaves m_Image pointing at unmapped memory; a subsequent caller reading through it is exploitable. A conformant implementation MUST verify len(pixels) exactly equals the header-derived expected size.
- The `size` field is a uint16[3]; the theoretical maximum volume is 65535^3 voxels. Combined with num_components = 255 and scalar_type = float64 (8 bytes), the theoretical maximum IMAGE payload is 255 * 65535^3 * 8 ≈ 574 exabytes — far beyond any reasonable buffer. Receivers SHOULD enforce an implementation-defined maximum on num_components * prod(subvol_size) * sizeof(scalar_type) before allocating.
- Upstream `igtl_image_convert_byte_order` (pinned SHA 94244fe) carries a TODO note about `-ftree-vectorize` possibly causing segfaults on 64-bit Linux — same construct as VIDEOMETA's byte-swap. A conformant reimplementation SHOULD byte-swap matrix elements in place without the stack uint32[12] round-trip.
- There is no separate 'IMAGE2' message type in the protocol; the file `Testing/img/igtlTestImage2.raw` in the upstream tree is simply the second test image used by the test suite.
- The `coord` field is rendering guidance, not a pixel transform: the pixel data itself is always laid out in i-fastest order regardless of coord system. Changing coord from RAS to LPS flips the interpretation of the matrix's orientation axes, not the pixel buffer layout.

**See also:** [`IMGMETA`](#imgmeta), [`COLORTABLE`](#colortable), [`VIDEO`](#video), [`TRANSFORM`](#transform), [`POSITION`](#position)

**Spec reference:** [protocol/v3.md §"Body (IMAGE)"](protocol/v3.md)

---

### `IMGMETA`

**Type ID:** `IMGMETA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Advertises the set of IMAGE volumes available on a server. Each element describes one image's name, device suffix, modality, patient identity, acquisition timestamp, spatial dimensions, and pixel type. A client uses IMGMETA to populate an image list without downloading full pixel data first.

**Rationale:** Image metadata is small (260 bytes per item) relative to image pixel data (often megabytes). IMGMETA lets a server publish 'here's what I have' as a cheap directory listing, letting clients browse before fetching. The per-element fields cover the DICOM-adjacent identifiers that most clinical consumers need for routing (modality, patient name/ID) without requiring a full DICOM header.

**Fields:**

**`images`** &nbsp;·&nbsp; `struct × (remaining)`

Array of 260-byte IMGMETA elements. Element count is derived as body_size / 260; a body whose size is not a multiple of 260 is malformed and MUST be rejected.

*Element fields:*

- **`name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 64 B — Human-readable image name or description, null-padded to 64 bytes.
- **`device_name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 20 B — Device-name suffix used to retrieve this IMAGE via GET_IMAGE. Forms the authoritative key for this metadata entry.
- **`modality`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 32 B — Imaging modality — e.g. 'CT', 'MR', 'US', 'XA'. Free-form ASCII; no enforced vocabulary. Implementations SHOULD use DICOM-standard modality codes where applicable.
- **`patient_name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 64 B — Patient name, null-padded to 64 bytes. This is PHI; implementations SHOULD treat the field as sensitive and MAY omit or redact it depending on deployment policy.
- **`patient_id`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 64 B — Patient identifier (e.g. MRN), null-padded to 64 bytes. Also PHI; same treatment guidance as patient_name.
- **`timestamp`** &nbsp;·&nbsp; `uint64` — Scan time as a uint64 in the same encoding as the OpenIGTLink header timestamp (upper 32 bits = seconds since Unix epoch, lower 32 bits = fraction-of-second). 0 means 'unspecified'.
- **`size`** &nbsp;·&nbsp; `uint16 × 3` — Entire image volume size in voxels: (Sx, Sy, Sz). Matches the dimensions the corresponding IMAGE message will report.
- **`scalar_type`** &nbsp;·&nbsp; `uint8` — Pixel scalar type, matching IMAGE's scalar_type codes: 2=int8, 3=uint8, 4=int16, 5=uint16, 6=int32, 7=uint32, 10=float32, 11=float64. Receivers MUST accept unknown values without rejection.
- **`reserved`** &nbsp;·&nbsp; `uint8` — Reserved; senders SHOULD write 0 and receivers MUST ignore.

**Legacy notes:**

- Total body size is exactly 260 * N where N is the number of image entries. An empty IMGMETA body (N=0, body_size=0) is legal and means 'no images available'.
- Patient name and patient ID fields carry PHI in most deployments. Implementations targeting HIPAA / GDPR environments SHOULD consider whether to redact these fields on outgoing IMGMETA and whether to reject incoming IMGMETA that carries PHI over an unencrypted transport. The wire format does not provide a redaction indicator; empty (null-padded) strings are the de facto redaction.
- Upstream C++ library (at pinned SHA 94244fe) iterates `nelem` elements in igtl_imgmeta_convert_byte_order without verifying the body is large enough. A conformant implementation MUST reject any IMGMETA message whose body_size is not a multiple of 260.

**See also:** [`LBMETA`](#lbmeta), [`IMAGE`](#image), [`COLORTABLE`](#colortable)

**Spec reference:** [protocol/v3.md §"Body (IMGMETA)"](protocol/v3.md)

---

### `LBMETA`

**Type ID:** `LBMETA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Advertises the set of label-map regions available as IMAGE messages on a server. Each element describes one label — its human name, the IMAGE device that carries the label data, the integer pixel value that represents the label, a color, the spatial extent, and an optional owner-image reference.

**Rationale:** A single IMAGE label-map can encode multiple labels (e.g. liver=1, kidney=2). LBMETA lets a server announce, per label, 'which pixel value is this named structure, what color should it render as, and which IMAGE do you fetch to get the pixels?' so a client can populate a segmentation list without downloading every IMAGE first.

**Fields:**

**`labels`** &nbsp;·&nbsp; `struct × (remaining)`

Array of 116-byte LBMETA elements. Element count is derived as body_size / 116; a body whose size is not a multiple of 116 is malformed and MUST be rejected.

*Element fields:*

- **`name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 64 B — Human-readable label name or description (e.g. 'Liver'). Null-padded to 64 bytes.
- **`device_name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 20 B — Device-name suffix of the IMAGE message carrying the label-map pixels. A client retrieves that IMAGE via GET_IMAGE using this name.
- **`label`** &nbsp;·&nbsp; `uint8` — Integer pixel value in the referenced IMAGE that corresponds to this label. Values are intended to be distinct within a single LBMETA element's referenced IMAGE, but that is not enforced at the wire level.
- **`reserved`** &nbsp;·&nbsp; `uint8` — Reserved; senders SHOULD write 0 and receivers MUST ignore.
- **`rgba`** &nbsp;·&nbsp; `uint8 × 4` — Suggested rendering color as RGBA bytes. (0, 0, 0, 0) is legal and interpreted as 'no color preference'.
- **`size`** &nbsp;·&nbsp; `uint16 × 3` — Extent of the label-map IMAGE in voxels: (Sx, Sy, Sz). Matches the referenced IMAGE's overall size.
- **`owner`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 20 B — Optional device-name suffix of the IMAGE that this label is a segmentation of (the 'anatomy' image the label-map was derived from). May be empty (all null bytes) when not applicable.

**Legacy notes:**

- Total body size is exactly 116 * N where N is the number of labels advertised. An empty LBMETA body (N=0, body_size=0) is legal and means 'no labels available'.
- Multiple LBMETA elements MAY reference the same IMAGE (via device_name) with different `label` values; this is the intended way to advertise a multi-label IMAGE (e.g. liver=1 AND kidney=2 pointing to the same label-map IMAGE). Duplicate (device_name, label) pairs are undefined.
- Upstream C++ library (at pinned SHA 94244fe) iterates `nelem` elements without verifying the body is large enough. A conformant implementation MUST reject any LBMETA message whose body_size is not a multiple of 116.

**See also:** [`IMGMETA`](#imgmeta), [`IMAGE`](#image), [`COLORTABLE`](#colortable)

**Spec reference:** [protocol/v3.md §"Body (LBMETA)"](protocol/v3.md)

---

### `NDARRAY`

**Type ID:** `NDARRAY` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Variable-rank N-dimensional numerical array. The body carries a 2-byte fixed header (scalar type + dimension count), a variable-length size table of uint16 values (one per axis), and then the raw array data as bytes. The data section contains product(size[0..dim-1]) elements of the declared scalar type, in row-major (C-contiguous) order.

**Rationale:** NDARRAY generalizes the pattern shared by IMAGE and SENSOR: it can transport any dense multi-dimensional numeric array without the spatial-orientation overhead of IMAGE. Typical uses include numerical results (Jacobians, Hessians, registration parameters), non-image grids (dose distributions), and generic matrix data. The variable rank and per-axis uint16 sizes make it flexible enough to handle 1D vectors through 255D tensors (though practical use rarely exceeds 4D).

**Fields:**

**`scalar_type`** &nbsp;·&nbsp; `uint8`

Scalar type code: 2=int8, 3=uint8, 4=int16, 5=uint16, 6=int32, 7=uint32, 10=float32, 11=float64, 13=complex (two float64 per element, 16 bytes). Receivers MUST reject unknown values; the upstream default branch returns bytes_per_element=0 for unknown types, which silently computes a data_size of 0.

**`dim`** &nbsp;·&nbsp; `uint8`

Number of dimensions (rank). Must be >= 1 for a meaningful array. dim=0 is legal on the wire (0 size entries, data section is empty) but degenerate. Determines the length of the following `size` array.

**`size`** &nbsp;·&nbsp; `uint16 × dim`

Per-axis extent: size[i] is the number of elements along the i-th axis. The total element count is the product of all size entries. Receivers MUST compute this product in 64-bit arithmetic (worst case: 255 axes of size 65535 each would overflow any smaller type) and MUST impose an implementation-defined ceiling before allocating.

**`data`** &nbsp;·&nbsp; `uint8 × (remaining)`

Raw array data — product(size[0..dim-1]) elements of `scalar_type`, stored in row-major order with the byte order determined by the OpenIGTLink convention (network / big-endian). The byte count MUST equal product(size) * bytes_per_scalar(scalar_type); receivers MUST verify this before accessing the data. The schema models the data as raw bytes (uint8[]) because the per-element width is determined dynamically by `scalar_type`.

**Post-unpack invariant:** `ndarray` — see `corpus-tools/src/oigtl_corpus_tools/codec/policy.py` for the cross-codec invariant definition.

**Legacy notes:**

- Total body_size = 2 + dim*2 + product(size) * bytes_per_scalar(scalar_type). The size table occupies dim × 2 bytes immediately after the 2-byte fixed header.
- NDARRAY adds scalar type code 13 (complex: two float64, 16 bytes per element) beyond the set shared by IMAGE / SENSOR / VIDEOMETA. Receivers that do not support complex SHOULD reject with STATUS rather than silently treating the data as 0-byte elements.
- Upstream igtl_ndarray_unpack (at pinned SHA 94244fe) carries an explicit TODO: '/* TODO: check if the pack size is valid */'. It does NOT validate pack_size against expected body size before reading the size table or the data section. An attacker setting dim=255 with body_size=2 causes a 510-byte OOB read of the size table alone, plus an unbounded OOB of the data section computed from the wild size values.
- Upstream igtl_ndarray_alloc_info computes len = product(size[i]) in a loop with uint64 intermediates but does not check for multiplication overflow. With dim=4 and all sizes=65535, len = 65535^4 ≈ 1.8×10^19 which overflows uint64. A conformant implementation MUST detect overflow and reject.
- The upstream byte-swap pass in igtl_ndarray_unpack modifies the receive buffer in place ('/* Change byte order -- this overwrites memory area for the pack !! */') and then 'restores' it afterward. This destructive in-place swap means the function is NOT idempotent; calling it twice on the same buffer produces corrupted data. A conformant reimplementation SHOULD swap into a separate output buffer.

**See also:** [`IMAGE`](#image), [`SENSOR`](#sensor)

**Spec reference:** [protocol/v3.md §"Body (NDARRAY)"](protocol/v3.md)

---

### `POINT`

**Type ID:** `POINT` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

List of 3D annotated points: landmarks, fiducials, surgical targets, and similar discrete spatial markers. Each element carries a point's name, group, color, 3D position, radius, and owning-image reference.

**Rationale:** A single message bundles many related points (e.g. 'all fiducials in this registration', 'all landmarks identified in this scan') so a client receives them atomically. The per-point group string lets a consumer partition points into semantically related sets (fiducials vs. landmarks vs. targets) without defining a group-level message type.

**Fields:**

**`points`** &nbsp;·&nbsp; `struct × (remaining)`

Array of 136-byte POINT elements. Element count is derived as body_size / 136; a body whose size is not a multiple of 136 is malformed and MUST be rejected.

*Element fields:*

- **`name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 64 B — Human-readable point name or description, null-padded to 64 bytes.
- **`group_name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 32 B — Group tag: 'Fiducial', 'Landmark', 'Labeled Point', or any application-specific string. Used to partition points into semantic sets.
- **`rgba`** &nbsp;·&nbsp; `uint8 × 4` — Suggested rendering color as RGBA bytes.
- **`position`** &nbsp;·&nbsp; `float32 × 3` — Point coordinates (x, y, z) in the session's reference frame.
- **`radius`** &nbsp;·&nbsp; `float32` — Rendering radius of the point (e.g. for a sphere glyph). A value of 0 means 'use renderer default' or 'point has no meaningful size'.
- **`owner`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 20 B — Optional device-name suffix of the IMAGE that this point is anchored to. May be empty (all null bytes) for points not tied to a specific image.

**Legacy notes:**

- Total body size is exactly 136 * N where N is the number of points. An empty POINT body (N=0, body_size=0) is legal and means 'no points'.
- The per-element struct packs with 1-byte alignment (no implicit padding). Implementations that rely on C struct layout without `#pragma pack(1)` will produce incorrect wire bytes on platforms that would otherwise pad `rgba`, `position`, or `radius`.
- Upstream C++ library (at pinned SHA 94244fe) iterates `nelem` elements in igtl_point_convert_byte_order without verifying the body is large enough. A conformant implementation MUST reject any POINT message whose body_size is not a multiple of 136.

**See also:** [`TRAJECTORY`](#trajectory), [`TDATA`](#tdata), [`QTDATA`](#qtdata)

**Spec reference:** [protocol/v3.md §"Body (POINT)"](protocol/v3.md)

---

### `POLYDATA`

**Type ID:** `POLYDATA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Polygonal mesh — points, topology (vertices, lines, polygons, triangle strips), and per-point or per-cell attributes. The body carries a 40-byte fixed header declaring counts and byte sizes for each section, followed by the data sections in a fixed order. Modeled after VTK's vtkPolyData.

**Rationale:** Surface meshes are the lingua franca of surgical planning and intraoperative visualization (organ surfaces, tool models, segmentation boundaries). POLYDATA transports VTK-style polygonal data directly: a point cloud plus topology arrays that index into it, plus optional per-point attributes (scalars, vectors, normals, tensors, texture coordinates, RGBA). Keeping the format VTK-compatible avoids a conversion step in the dominant consumer (3D Slicer).

**Fields:**

**`npoints`** &nbsp;·&nbsp; `uint32`

Number of 3D points. Each point occupies 3 × float32 = 12 bytes in the points section.

**`nvertices`** &nbsp;·&nbsp; `uint32`

Number of vertex primitives in the vertices topology section. This is the number of VTK-style 'cells' of type vertex, not the number of uint32 values in the section (which is given by size_vertices / 4).

**`size_vertices`** &nbsp;·&nbsp; `uint32`

Byte size of the vertices topology section. MUST be a multiple of 4 (each entry is a uint32). The section contains VTK-style connectivity: sequences of (N, i1, i2, ..., iN) where N is the point count per cell and i1..iN are 0-based point indices.

**`nlines`** &nbsp;·&nbsp; `uint32`

Number of line primitives.

**`size_lines`** &nbsp;·&nbsp; `uint32`

Byte size of the lines topology section. MUST be a multiple of 4.

**`npolygons`** &nbsp;·&nbsp; `uint32`

Number of polygon primitives.

**`size_polygons`** &nbsp;·&nbsp; `uint32`

Byte size of the polygons topology section. MUST be a multiple of 4.

**`ntriangle_strips`** &nbsp;·&nbsp; `uint32`

Number of triangle-strip primitives.

**`size_triangle_strips`** &nbsp;·&nbsp; `uint32`

Byte size of the triangle strips topology section. MUST be a multiple of 4.

**`nattributes`** &nbsp;·&nbsp; `uint32`

Number of per-point or per-cell attribute arrays. Determines the length of the `attribute_headers` array and the number of entries in the trailing attribute name table and data arrays.

**`points`** &nbsp;·&nbsp; `struct × npoints`

Array of npoints × 12-byte (x, y, z) coordinates. Point indices in the topology sections are 0-based offsets into this array.

*Element fields:*

- **`x`** &nbsp;·&nbsp; `float32` — X coordinate in millimeters.
- **`y`** &nbsp;·&nbsp; `float32` — Y coordinate in millimeters.
- **`z`** &nbsp;·&nbsp; `float32` — Z coordinate in millimeters.

**`vertices`** &nbsp;·&nbsp; `uint8 × size_vertices`

Vertices topology section — size_vertices raw bytes of VTK-style uint32 connectivity data: sequences of (N, i1, i2, ..., iN). Modeled as raw bytes because the per-cell structure (variable-length runs) cannot be expressed as a uniform array. Receivers MUST interpret as uint32[] in network byte order after verifying size_vertices % 4 == 0.

**`lines`** &nbsp;·&nbsp; `uint8 × size_lines`

Lines topology section — size_lines raw bytes. Same encoding as vertices.

**`polygons`** &nbsp;·&nbsp; `uint8 × size_polygons`

Polygons topology section — size_polygons raw bytes. Same encoding as vertices.

**`triangle_strips`** &nbsp;·&nbsp; `uint8 × size_triangle_strips`

Triangle strips topology section — size_triangle_strips raw bytes. Same encoding as vertices.

**`attribute_headers`** &nbsp;·&nbsp; `struct × nattributes`

Array of nattributes × 6-byte attribute headers. Each entry declares the type, component count, and tuple count for one attribute array. The corresponding names and data follow in the trailing sections.

*Element fields:*

- **`type`** &nbsp;·&nbsp; `uint8` — Attribute type: 0x00=POINT_DATA/Scalars, 0x01=POINT_DATA/Vectors, 0x02=POINT_DATA/Normals, 0x03=POINT_DATA/Tensors, 0x04=RGBA, 0x05=Texture Coordinates. Add 0x10 for CELL_DATA variants (0x10=CELL_DATA/Scalars, etc.). Receivers MUST accept unknown values gracefully.
- **`ncomponents`** &nbsp;·&nbsp; `uint8` — Number of scalar components per tuple. 1 for Scalars, 3 for Vectors/Normals, 9 for Tensors. For Scalars, this is the user-specified component count from the sender. For other types, the sender SHOULD write the canonical value but receivers MUST derive the actual component count from `type` if they conflict.
- **`n`** &nbsp;·&nbsp; `uint32` — Number of tuples in this attribute array. The data section for this attribute contains n × ncomponents × sizeof(float32) bytes. For POINT_DATA types, n should equal npoints; for CELL_DATA types, n should equal the relevant cell count.

**`attribute_data`** &nbsp;·&nbsp; `uint8 × (remaining)`

Attribute name table + attribute data arrays, concatenated. The name table comes first: nattributes null-terminated ASCII strings (max 255 chars each), packed together, with a padding byte if the total byte count is odd. Then, for each attribute i in order, n[i] × ncomponents[i] × sizeof(float32) bytes of float32 data in network byte order. Receivers MUST parse using the attribute_headers array to determine boundaries; without it, the blob is not self-describing.

**Post-unpack invariant:** `polydata` — see `corpus-tools/src/oigtl_corpus_tools/codec/policy.py` for the cross-codec invariant definition.

**Legacy notes:**

- The fixed header is exactly 40 bytes (10 × uint32). Total body_size = 40 + npoints*12 + size_vertices + size_lines + size_polygons + size_triangle_strips + nattributes*6 + attribute_name_table_bytes(padded) + sum(n[i]*ncomponents[i]*4).
- Topology sections use VTK connectivity encoding: a sequence of runs (N, i1, i2, ..., iN) where N is the number of point indices in the cell and i1..iN are 0-based indices into the points array. This is NOT a flat index array; N is interleaved with the indices. The byte sizes in the header (size_vertices etc.) include the N values.
- All topology section sizes MUST be multiples of 4 (uint32 alignment). Upstream igtl_polydata_unpack checks this at lines 349-356 and returns 0 on violation — one of the few actual validation checks in the upstream code.
- Upstream igtl_polydata_unpack (at pinned SHA 94244fe) does NOT validate body_size against the 40-byte header before casting to igtl_polydata_header*. Attacker-controlled npoints with a small body causes igtl_polydata_alloc_info to call malloc(npoints * 12) — a 32-bit uint32 overflow for npoints > ~357M yields an under-sized allocation, followed by an OOB write in the byte-swap loop.
- Attribute name parsing uses strlen(ptr) walking forward with no bounds check against remaining body bytes (lines 397-409). An attacker-crafted message can make the parser walk past the attribute data section into unmapped memory.
- Upstream igtl_polydata_get_crc truncates the uint64 data_size to igtl_uint32 at line 745: `polydata_length = (igtl_uint32)igtl_polydata_get_size(...)`. Any mesh whose body exceeds 4 GiB computes CRC over the wrong byte count. A conformant implementation MUST use the full uint64 size.
- Attribute data is always float32 regardless of attribute type. Scalars, vectors, normals, tensors, texture coordinates, and RGBA are all encoded as float32 arrays. RGBA values are NOT packed uint8[4] — they are 4 × float32 per tuple (likely in [0.0, 1.0] range), consistent with VTK's vtkFloatArray representation.
- Attribute types 0x04 (RGBA) and 0x05 (TCOORDS/Texture Coordinates) are defined in the header but the upstream unpack function does not handle them in its ncomponents switch — they fall through to the TENSOR default (9 components). The pack function handles TCOORDS (3 components) but not RGBA. Conformant implementations SHOULD use: RGBA=4 components, TCOORDS=2 or 3 components (matching VTK).

**See also:** [`IMAGE`](#image), [`POINT`](#point), [`TRANSFORM`](#transform)

**Spec reference:** [protocol/v3.md §"Body (POLYDATA)"](protocol/v3.md)

---

### `POSITION`

**Type ID:** `POSITION` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

3D position with an optional orientation quaternion. The body carries a float32[3] position vector always, plus 0, 3, or 4 additional float32 values for orientation. Body size is exactly 12 (position only), 24 (position + 3-element compressed quaternion), or 28 (position + full 4-element quaternion). Any other body size is malformed.

**Rationale:** POSITION predates TRANSFORM and was intended for the common case of a single tracked tool or pointer. Its three body-size variants let a sender omit orientation when it has no rotational information (12 bytes) or send a compressed quaternion when the fourth component is reconstructible from unit-norm constraint (24 bytes). In practice, most modern deployments send the full 28-byte form or use TRANSFORM instead. The message type is preserved for backward compatibility.

**Fields:**

**`position`** &nbsp;·&nbsp; `float32 × 3`

Position coordinates (x, y, z) in millimeters. Always present regardless of body size.

**`quaternion`** &nbsp;·&nbsp; `float32 × (remaining)`

Orientation quaternion. The number of elements depends on body_size: 0 elements (body=12, position only — no orientation), 3 elements (body=24, compressed quaternion: (qx, qy, qz) with qw reconstructed as sqrt(1 - qx² - qy² - qz²)), or 4 elements (body=28, full quaternion: (qx, qy, qz, qw)). Any other count (1, 2, 5+) is malformed and MUST be rejected. Receivers MUST normalize the reconstructed or received quaternion before use.

**Legal body sizes:** 12, 24, 28 bytes only. Codecs reject any other length before field access.

**Legacy notes:**

- Valid body sizes are exactly {12, 24, 28}. A conformant implementation MUST reject any POSITION message whose body_size is not in this set before any field access. The schema expresses this via count_from=remaining on the quaternion field; the receiver additionally validates the resulting element count ∈ {0, 3, 4}.
- Upstream C provides three separate byte_order functions: igtl_position_convert_byte_order (28 B, swaps position[3] + quaternion[4]), igtl_position_convert_byte_order_position_only (12 B, swaps position[3]), igtl_position_convert_byte_order_quaternion3 (24 B, swaps position[3] + quaternion[3]). The caller must select the correct variant based on body_size; there is no dispatch wrapper. A conformant reimplementation SHOULD dispatch in one function conditioned on body_size.
- Upstream igtl_position_get_crc always computes CRC over IGTL_POSITION_MESSAGE_DEFAULT_SIZE (28 bytes) regardless of actual body_size. This means the CRC for a 12- or 24-byte message includes uninitialized trailing bytes — a bug. A conformant implementation MUST compute CRC over exactly body_size bytes.
- Upstream PositionMessage::UnpackContent (at pinned SHA 94244fe) does not validate body_size before reading fields. A body shorter than 12 bytes causes an OOB read. The 24 vs. 28 discrimination is done after reading all 28 bytes, meaning a 24-byte message reads 4 bytes past the body end.
- The QTRANS message type (used internally by some codepaths for compact quaternion+translation) is a utility concept, not a distinct wire message type. Its wire representation is a subset of POSITION's 24-byte variant.

**See also:** [`TRANSFORM`](#transform), [`TDATA`](#tdata), [`QTDATA`](#qtdata)

**Spec reference:** [protocol/v3.md §"Body (POSITION)"](protocol/v3.md)

---

### `QTDATA`

**Type ID:** `QTDATA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Stream of tracked-tool poses using position + quaternion representation. Each element reports one tool's name, classification, 3D position, and orientation quaternion. Typical use: real-time tracking of surgical instruments at 30–240 Hz where quaternion orientation is preferred over a full transformation matrix.

**Rationale:** QTDATA is the quaternion-orientation counterpart to TDATA. Each element is 50 bytes versus TDATA's 70, saving bandwidth when orientation can be expressed compactly. Multiple tools may be packed into a single QTDATA message so a multi-tool tracking snapshot arrives atomically.

**Fields:**

**`tools`** &nbsp;·&nbsp; `struct × (remaining)`

Array of 50-byte QTDATA elements. Element count is derived as body_size / 50; a body whose size is not a multiple of 50 is malformed and MUST be rejected (see U-7 in protocol/v3.md).

*Element fields:*

- **`name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 20 B — Instrument or tracker name, null-padded to 20 bytes.
- **`type`** &nbsp;·&nbsp; `uint8` — Tool kind: 1 = tracker reference, 2 = 6D instrument, 3 = 3D instrument (tip only), 4 = 5D instrument (tip + handle). Receivers MUST accept unknown values without rejection.
- **`reserved`** &nbsp;·&nbsp; `uint8` — Reserved; senders SHOULD write 0 and receivers MUST ignore.
- **`position`** &nbsp;·&nbsp; `float32 × 3` — Tool position (x, y, z) in the session's reference frame.
- **`quaternion`** &nbsp;·&nbsp; `float32 × 4` — Tool orientation as a quaternion (qx, qy, qz, w). Unit-magnitude is expected; implementations SHOULD tolerate small deviations from unit magnitude.

**Legacy notes:**

- Total body size is exactly 50 * N where N is the number of tools reported. An empty message (N=0, body_size=0) is legal but unusual.
- The `type` byte is intentionally a numeric classification rather than a free-text tag so receivers can discriminate without parsing. Additions to the type set over time are additive; unknown values are ignored at the receiver (see description).
- Upstream C++ library (at pinned SHA 94244fe) iterates `nelem` elements in igtl_qtdata_convert_byte_order without verifying the body is large enough. A conformant implementation MUST reject any QTDATA message whose body_size is not a multiple of 50, and MUST reject messages where body_size < expected_count * 50 before any element access. See partial-element rejection hardening (U-7 class).

**See also:** [`TDATA`](#tdata), [`POSITION`](#position), [`TRANSFORM`](#transform)

**Spec reference:** [protocol/v3.md §"Body (QTDATA)"](protocol/v3.md)

---

### `QTRANS`

**Type ID:** `QTRANS` &nbsp;·&nbsp; **Introduced in:** v3 &nbsp;·&nbsp; **Body size:** 28 bytes &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Position + full quaternion orientation in 28 bytes fixed. Wire-level equivalent to the 28-byte variant of POSITION, but distinct at the type_id level so receivers can dispatch deterministically without reading body_size to choose between POSITION variants. Useful for high-rate tracking where the 19% size saving over TRANSFORM matters and where senders prefer a fixed-layout message with no size-discriminated variants.

**Rationale:** POSITION's three body-size variants (12/24/28) require receivers to inspect body_size before dispatching to the right byte-swap routine — error-prone and a frequent source of framing bugs. QTRANS commits to the full-quaternion form in a single fixed layout, which is safer for dispatch and friendlier to codegen. It is listed as a distinct wire type_id in Documents/Protocol/qtransform.md (v3.0, January 2017) even though the upstream C++ reference implementation does not provide a dedicated class for it (users of the upstream library emit POSITION with the 28-byte variant instead).

**Fields:**

**`position`** &nbsp;·&nbsp; `float32 × 3`

Position coordinates (x, y, z) in millimeters.

**`quaternion`** &nbsp;·&nbsp; `float32 × 4`

Orientation as quaternion (qx, qy, qz, qw). The sender SHOULD normalize before transmission; the receiver MUST normalize before use.

**Legacy notes:**

- QTRANS is a spec-level wire message type (v3.0, qtransform.md) but has no dedicated class in the upstream C++ library as of SHA 94244fe. Upstream implementations send an equivalent payload as POSITION with body_size=28. Receivers that want full spec compatibility SHOULD accept both type_ids and treat them as semantically equivalent.
- Unlike POSITION, QTRANS has no size variants — the body is always exactly 28 bytes. Receivers MUST reject any QTRANS message with body_size != 28.
- The field layout matches POSITION's 28-byte variant byte-for-byte: the first 12 bytes are position, the next 16 are quaternion. A receiver that decodes POSITION's 28-byte variant can decode QTRANS with identical code.

**See also:** [`POSITION`](#position), [`TRANSFORM`](#transform), [`QTDATA`](#qtdata)

**Spec reference:** [protocol/v3.md §"Body (QTRANS)"](protocol/v3.md)

---

### `SENSOR`

**Type ID:** `SENSOR` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Reports a vector of scalar sensor readings with an attached SI unit-of-measure descriptor. Typical use: multi-channel physiological monitors, force/torque sensors, analog-to-digital acquisition devices.

**Rationale:** Separates 'how many samples' (larray), 'what they mean' (the unit descriptor), and 'their values' (the float64 array) so a receiver can interpret readings without device-specific context. The unit field is bit-packed into a single uint64 so every reading is self-describing with a minimum of wire overhead.

**Fields:**

**`larray`** &nbsp;·&nbsp; `uint8`

Number of float64 sensor values that follow. Range 0..255. Caps the maximum body size at 10 + 255*8 = 2050 bytes.

**`status`** &nbsp;·&nbsp; `uint8`

Reserved sensor-status byte. No standardized values; receivers SHOULD ignore unrecognized values and MUST NOT reject a message based on its content.

**`unit`** &nbsp;·&nbsp; `uint64`

Bit-packed SI-unit descriptor encoding a decimal prefix and up to six (SI-base or SI-derived) unit codes with signed exponents. See the unit-packing section of protocol/v3.md for the bit layout. A unit value of 0 is treated as 'unspecified'. Treated at the wire level as a single big-endian uint64; the bit layout is the codec's concern, not the parser's.

*Legacy:* The unit field's internal layout is defined by igtl_unit_pack / igtl_unit_unpack in the reference library (igtl_unit.h). Prefix in the high bits, then 6 * 4-bit unit codes, then 6 * 4-bit signed exponents (range -7..7). Conformant implementations MUST preserve the uint64 on the wire exactly; decoding the substructure is optional and can be deferred to a helper.

**`data`** &nbsp;·&nbsp; `float64 × larray`

Array of `larray` big-endian IEEE-754 float64 sensor readings. Units are given by the preceding `unit` field; the readings are numerically meaningful only in that unit.

**Legacy notes:**

- Total body size is always 10 + larray * 8. An empty sensor reading (larray=0) is legal and gives a 10-byte body.
- Upstream C++ library (at pinned SHA 94244fe) unpacks this message without verifying that body_size >= 10 + larray * 8 before calling igtl_sensor_convert_byte_order, which byte-swaps `larray` float64s in place. A conformant implementation MUST reject any SENSOR message whose body_size is less than 10 + larray*8 before any data-array access. Otherwise an attacker-controlled larray with a short body causes OOB read/write of up to 2040 bytes past the receive buffer.

**See also:** [`TDATA`](#tdata), [`QTDATA`](#qtdata)

**Spec reference:** [protocol/v3.md §"Body (SENSOR)"](protocol/v3.md)

---

### `STATUS`

**Type ID:** `STATUS` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Reports the operational status of a device or the outcome of a request. Carries a numeric status code, a device-specific subcode, a short error name, and an optional free-text status message.

**Rationale:** Provides a structured, machine-readable way for devices to report success, errors, and lifecycle events (starting up, shutting down, manual mode, hardware failure, etc.) to their peers. The subcode is intentionally device-specific so vendors can encode additional detail without standardizing new status codes.

**Fields:**

**`code`** &nbsp;·&nbsp; `uint16`

Status code. See the STATUS code table in protocol/v3.md. 0 = invalid, 1 = OK, 2 = unknown error, and values up to 19 for specific failure modes. Receivers MUST accept unknown code values rather than rejecting the message, so the set can grow without breaking compatibility.

**`subcode`** &nbsp;·&nbsp; `int64`

Device-specific subcode providing additional detail about the status condition. Not centrally standardized; interpretation is defined by the sending device's documentation.

**`error_name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 20 B

Short human-readable name for the error condition, null-padded to 20 bytes. Optional: implementations may leave this empty (all zeros). MUST NOT be relied upon for machine dispatch — use `code` and `subcode` for that.

*Legacy:* Upstream C++ library (at pinned SHA 94244fe) writes this field with strncpy(dst, src, 20) without guaranteeing null termination if the input is 20 or more characters. A conformant implementation MUST either reserve byte 19 as a hard null terminator (accepting a 19-character effective limit) or treat the field as fixed 20 bytes without any null-termination expectation. Receivers MUST defensively null-terminate before using the field as a C string.

**`status_message`** &nbsp;·&nbsp; `trailing_string`

Free-text human-readable status message. Occupies all remaining bytes of the body after the 30-byte fixed header. The final byte on the wire MUST be 0x00; the string value is the bytes before that terminator (which may be empty). Total body size = 30 + len(status_message) + 1.

*Legacy:* Upstream C++ library (at pinned SHA 94244fe) will silently discard the status_message on unpack if the trailing null is missing, but still produces garbage from an undersized body. Conformant receivers MUST reject messages whose body_size < 31 and MUST reject messages whose last content byte is non-null.

**Legacy notes:**

- The fixed portion of the body is 30 bytes: uint16 code + int64 subcode + char[20] error_name, struct-packed with no padding. Implementations MUST NOT rely on #pragma pack or equivalent for serialization; emit the three fields in order with the exact sizes declared here.

**See also:** [`COMMAND`](#command), [`STRING`](#string)

**Spec reference:** [protocol/v3.md §"Body (STATUS)"](protocol/v3.md)

---

### `STRING`

**Type ID:** `STRING` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Carries a character string with an explicit character-encoding hint. The payload is a plain byte sequence whose interpretation is governed by the leading encoding field.

**Rationale:** Provides a device-to-host text channel (notifications, parameters, free-text command arguments) that can carry non-ASCII content. Uses an explicit IANA MIBenum encoding field rather than assuming a single charset because medical devices frequently emit vendor-specific or localized text.

**Fields:**

**`encoding`** &nbsp;·&nbsp; `uint16`

Character encoding as an IANA MIBenum value. Typical: 3 (US-ASCII), 106 (UTF-8). See the IANA character-sets registry at https://www.iana.org/assignments/character-sets. Receivers MUST accept unknown encoding values without rejection; if the value is not recognized, the payload MAY be treated as opaque bytes.

**`value`** &nbsp;·&nbsp; `length_prefixed_string`

String payload: a big-endian uint16 length, immediately followed by that many bytes of string data. Interpretation of the bytes is governed by the preceding `encoding` field; at the wire level the field is opaque. The 16-bit length caps the maximum string size at 65535 bytes. No terminator byte.

*Legacy:* Upstream C++ library (at pinned SHA 94244fe) unpacks this field with m_String.append(ptr, header->length) on line 135 of igtlStringMessage.cxx without bounds-checking `length` against the actual body size available. A conformant implementation MUST reject any STRING message where body_size < 4 + length; otherwise an attacker-controlled length value causes OOB reads up to 65535 bytes past the buffer.

**Legacy notes:**

- Total body size is 4 + length bytes: the fixed 4-byte header (uint16 encoding + uint16 length) followed by `length` bytes of payload. An empty string is legal (length=0, total body_size=4).
- There is no null terminator. Implementations MUST NOT assume one is present or append one.

**See also:** [`STATUS`](#status), [`COMMAND`](#command)

**Spec reference:** [protocol/v3.md §"Body (STRING)"](protocol/v3.md)

---

### `TDATA`

**Type ID:** `TDATA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Stream of tracked-tool poses using a 3×4 transformation matrix per tool. Each element reports one tool's name, classification, and full pose as 12 floats in the same column-major layout as TRANSFORM. Typical use: real-time tracking of surgical instruments at 30–240 Hz when the sender prefers matrix orientation over quaternion.

**Rationale:** TDATA is the matrix-orientation counterpart to QTDATA. Each element carries the identical 48-byte transformation body as the TRANSFORM message, plus 22 bytes of per-tool identification and classification. Multiple tools may be packed into a single TDATA message so a multi-tool tracking snapshot arrives atomically. At 70 bytes per tool, TDATA is ~40% larger per tool than QTDATA — the tradeoff is that no orientation decomposition is required at the receiver.

**Fields:**

**`tools`** &nbsp;·&nbsp; `struct × (remaining)`

Array of 70-byte TDATA elements. Element count is derived as body_size / 70; a body whose size is not a multiple of 70 is malformed and MUST be rejected.

*Element fields:*

- **`name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 20 B — Instrument or tracker name, null-padded to 20 bytes.
- **`type`** &nbsp;·&nbsp; `uint8` — Tool kind: 1 = tracker reference, 2 = 6D instrument, 3 = 3D instrument (tip only), 4 = 5D instrument (tip + handle). Receivers MUST accept unknown values without rejection.
- **`reserved`** &nbsp;·&nbsp; `uint8` — Reserved; senders SHOULD write 0 and receivers MUST ignore.
- **`transform`** &nbsp;·&nbsp; `float32 × 12` — Upper-3×4 of a 4×4 homogeneous transformation matrix, 12 floats in column-major order. Wire order: R11, R21, R31, R12, R22, R32, R13, R23, R33, TX, TY, TZ. Same encoding as TRANSFORM's body — see TRANSFORM's legacy_notes for the column-major interop trap.

**Legacy notes:**

- Total body size is exactly 70 * N where N is the number of tools reported. An empty message (N=0, body_size=0) is legal but unusual.
- The 48-byte `transform` sub-field is byte-identical to the body of a TRANSFORM message. Implementations SHOULD share the encode/decode path between TRANSFORM and TDATA elements; the column-major layout trap that bit openigtlink-rust v0.3.x applies equally to TDATA.
- Upstream C++ library (at pinned SHA 94244fe) iterates `nelem` elements in igtl_tdata_convert_byte_order without verifying the body is large enough. A conformant implementation MUST reject any TDATA message whose body_size is not a multiple of 70, and MUST reject messages where body_size < expected_count * 70 before any element access.

**See also:** [`QTDATA`](#qtdata), [`TRANSFORM`](#transform)

**Spec reference:** [protocol/v3.md §"Body (TDATA)"](protocol/v3.md)

---

### `TRAJ`

**Type ID:** `TRAJ` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

List of 3D trajectories — planned or executed paths from an entry point to a target point. Typical use: planning needle paths, catheter insertions, and surgical access routes. Each element carries a trajectory's name, type, color, entry/target positions, radius, and owning-image reference.

**Rationale:** Trajectories are essentially two points (entry and target) plus classification metadata. A dedicated message type captures the entry/target relationship explicitly rather than leaving it implicit in a POINT pair, which matters for workflow integrations that reason about insertion paths (collision avoidance, pullback visualization).

**Fields:**

**`trajectories`** &nbsp;·&nbsp; `struct × (remaining)`

Array of 150-byte TRAJECTORY elements. Element count is derived as body_size / 150; a body whose size is not a multiple of 150 is malformed and MUST be rejected.

*Element fields:*

- **`name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 64 B — Human-readable trajectory name or description, null-padded to 64 bytes.
- **`group_name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 32 B — Group tag (e.g. 'Trajectory', 'Planned Path', or any application-specific string).
- **`type`** &nbsp;·&nbsp; `int8` — Which endpoints are populated: 1 = entry-only (target_pos ignored), 2 = target-only (entry_pos ignored), 3 = entry-and-target. Receivers MUST accept unknown values without rejection and SHOULD treat them as equivalent to 'entry-and-target'.
- **`reserved`** &nbsp;·&nbsp; `int8` — Reserved; senders SHOULD write 0 and receivers MUST ignore.
- **`rgba`** &nbsp;·&nbsp; `uint8 × 4` — Suggested rendering color as RGBA bytes.
- **`entry_pos`** &nbsp;·&nbsp; `float32 × 3` — Entry-point coordinates (x, y, z). Meaningful when `type` indicates entry is defined; undefined otherwise.
- **`target_pos`** &nbsp;·&nbsp; `float32 × 3` — Target-point coordinates (x, y, z). Meaningful when `type` indicates target is defined; undefined otherwise.
- **`radius`** &nbsp;·&nbsp; `float32` — Rendering radius of the trajectory (e.g. for a tube or cylinder glyph). A value of 0 means 'use renderer default' or 'no meaningful radius'.
- **`owner_name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 20 B — Optional device-name suffix of the IMAGE this trajectory is anchored to. May be empty (all null bytes).

**Legacy notes:**

- Total body size is exactly 150 * N where N is the number of trajectories. An empty TRAJECTORY body (N=0, body_size=0) is legal.
- The per-element struct packs with 1-byte alignment. Implementations that rely on C struct layout without `#pragma pack(1)` will produce incorrect wire bytes on platforms that would otherwise pad the rgba/entry_pos/target_pos/radius fields.
- The `type` field determines which of entry_pos / target_pos is semantically valid, but BOTH occupy wire bytes regardless. Senders SHOULD write zeros for the unused position but receivers MUST NOT depend on that.
- Upstream C++ library (at pinned SHA 94244fe) iterates `nelem` elements without verifying the body is large enough. A conformant implementation MUST reject any TRAJECTORY message whose body_size is not a multiple of 150.

**See also:** [`POINT`](#point), [`TDATA`](#tdata)

**Spec reference:** [protocol/v3.md §"Body (TRAJECTORY)"](protocol/v3.md)

---

### `TRANSFORM`

**Type ID:** `TRANSFORM` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 48 bytes &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

4x4 homogeneous transformation matrix in a right-handed coordinate system. Used to communicate a single rigid-body pose — for example, a tracked tool's position and orientation.

**Rationale:** Only the upper 3x4 submatrix is transmitted (12 floats = 48 bytes). The bottom row of a homogeneous transformation is always [0, 0, 0, 1] for rigid transforms, so omitting it saves bandwidth at 60Hz+ tracking rates with no loss of information.

**Fields:**

**`matrix`** &nbsp;·&nbsp; `float32 × 12` &nbsp;·&nbsp; 48 B

Twelve 32-bit floats in column-major order, encoding the upper 3x4 of a 4x4 homogeneous transformation matrix. Wire order is R11, R21, R31, R12, R22, R32, R13, R23, R33, TX, TY, TZ — that is, three rotation-matrix columns followed by the translation column.

*Rationale:* Column-major ordering chosen in v1 for alignment with graphics-convention matrix storage. The 3x4 convention (rather than 4x3) makes translation the final three floats, which some hand-decoded test traces rely on.

*Layout:* `column_major_3x4`.

*Legacy:* v1: wire order is column-major. At least one independent implementation (openigtlink-rust prior to v0.4.0) initially encoded row-major and produced bytes incompatible with the C++ reference. This is the single most common interop bug for new implementations and is pinned by the conformance corpus.

**Legacy notes:**

- The implicit bottom row [0, 0, 0, 1] is NOT transmitted. Implementations that receive a TRANSFORM message and hold a full 4x4 must set the bottom row explicitly after decoding. Implementations that expose a 3x4-only API can skip this step.
- The 48-byte body size is constant across v1, v2, and v3 for this message type. In v2 and v3, metadata may follow the body content; in v3 an extended header may precede it.

**See also:** [`POSITION`](#position), [`QTRANS`](#qtrans), [`TDATA`](#tdata), [`QTDATA`](#qtdata)

**Spec reference:** [protocol/v3.md §"Body (TRANSFORM)"](protocol/v3.md)

---

### `VIDEO`

**Type ID:** `VIDEO` &nbsp;·&nbsp; **Introduced in:** v3 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Compressed video frame with in-band orientation. The body is a 76-byte frame header followed by the codec-compressed frame payload. Structurally similar to IMAGE but with a FourCC codec identifier instead of scalar_type/num_components, and no meaningful byte-count relationship between subvol_size and the compressed frame data (the frame payload size is determined by the codec, not by the pixel dimensions).

**Rationale:** Streaming uncompressed video over OpenIGTLink (via IMAGE messages) wastes bandwidth for real-time endoscopy, laparoscopy, and ultrasound use cases. VIDEO wraps a codec-compressed frame (H.264, VP9, AV1, etc.) with the same spatial metadata (matrix, subvolume) as IMAGE so a receiver can place the decoded frame in 3D scene space. The FourCC codec field lets a receiver identify the decompressor needed before reading any frame bytes.

**Fields:**

**`header_version`** &nbsp;·&nbsp; `uint16`

VIDEO header format version, currently 1. Distinct from the OpenIGTLink protocol version. Receivers MUST reject values they do not implement.

**`endian`** &nbsp;·&nbsp; `uint8`

Byte order of the decoded pixel data: 1=big, 2=little. Applies to the pixel interpretation after codec decompression, not to the compressed frame payload itself (which is opaque bytes).

**`codec`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 4 B

FourCC codec identifier: 'I420' (uncompressed YUV 4:2:0), 'H264' (H.264/AVC), 'VP90' (VP9), 'X265' (H.265/HEVC via x265), 'O265' (H.265/HEVC via OpenHEVC), 'AV10' (AV1). Receivers MUST reject unknown codec values with STATUS rather than attempting to interpret the frame data.

**`frame_type`** &nbsp;·&nbsp; `uint16`

Codec-specific frame type indicator (e.g. I-frame, P-frame, B-frame). Semantics depend on the codec identified by `codec`; the protocol treats this as an opaque hint. 0 is a safe default for codecs that don't distinguish frame types.

**`coord`** &nbsp;·&nbsp; `uint8`

Coordinate-system convention: 1=RAS, 2=LPS. Same semantics as IMAGE's `coord` field.

**`size`** &nbsp;·&nbsp; `uint16 × 3`

Full video frame dimensions in pixels: (Sx, Sy, Sz). For 2D video, Sz=1.

**`matrix`** &nbsp;·&nbsp; `float32 × 12`

Frame-plane orientation and origin. Same layout as IMAGE: four float32[3] groups — norm_i·pixel_size_i, norm_j·pixel_size_j, norm_k·pixel_size_k, origin (mm).

**`subvol_offset`** &nbsp;·&nbsp; `uint16 × 3`

Sub-region offset within the full frame: (Ox, Oy, Oz). Same semantics as IMAGE's subvol_offset.

**`subvol_size`** &nbsp;·&nbsp; `uint16 × 3`

Sub-region extent: (Wx, Wy, Wz). Unlike IMAGE, the trailing frame_data byte count is NOT derivable from subvol_size because the data is codec-compressed. Receivers determine frame_data length from body_size - 76.

**`frame_data`** &nbsp;·&nbsp; `uint8 × (remaining)`

Codec-compressed frame payload. Byte count = body_size - 76. The receiver must decompress this using the codec identified by the `codec` field. For 'I420' (uncompressed), the byte count has a fixed relationship to subvol_size; for all other codecs it does not.

**Legacy notes:**

- VIDEO was introduced in protocol v3 alongside the VideoStreaming extension. The 76-byte header mirrors IMAGE's 72-byte header with two changes: (1) scalar_type/num_components are replaced by a 4-byte FourCC codec field + a 2-byte frame_type, and (2) the header_version and endian fields are reordered.
- Total body_size = 76 + compressed_frame_bytes. An empty frame (body_size = 76, 0 bytes of frame_data) is legal and may indicate 'no new frame available' or a signaling-only message.
- The `codec` field is a 4-byte ASCII FourCC, NOT null-padded — all four bytes are significant. 'VP90' means VP9 (trailing zero is part of the code), 'AV10' means AV1. Implementations MUST compare all four bytes.
- Upstream igtl_frame_convert_byte_order (at pinned SHA 94244fe) does NOT validate body_size before reading the 76-byte header. A body shorter than 76 bytes causes an OOB read in the byte-swap pass. A conformant implementation MUST reject body_size < 76 before any header access.
- The frame_type field's semantics are codec-dependent and not standardized by the protocol. Upstream code uses values like 0x0001 for I-frame; consumers SHOULD NOT rely on specific frame_type values without knowing the codec.
- STT_VIDEO (start streaming) is a companion message type with its own 9-byte body: codec[4] (FourCC) + time_interval (uint32, milliseconds between frames) + a reserved byte. It is NOT part of the VIDEO schema but is documented here for completeness.

**See also:** [`IMAGE`](#image), [`VIDEOMETA`](#videometa), [`TRANSFORM`](#transform)

**Spec reference:** [protocol/v3.md §"Body (VIDEO)"](protocol/v3.md)

---

### `VIDEOMETA`

**Type ID:** `VIDEOMETA` &nbsp;·&nbsp; **Introduced in:** v3 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Advertises the set of VIDEO sources available on a server. Each element describes one video stream's name, device suffix, patient identity, camera parameters (zoom level and focal length), frame-plane orientation matrix, frame dimensions, and pixel type. A client uses VIDEOMETA to populate a video source list without subscribing to any stream first.

**Rationale:** Video sources carry both patient-identifying metadata (like IMGMETA) and camera-specific parameters (zoom, focal length) that a consumer needs to interpret the resulting VIDEO frames. The per-element matrix captures frame-plane orientation the same way IMAGE does, so a video frame can be placed in 3D scene space without a separate TRANSFORM lookup. VIDEOMETA is to VIDEO what IMGMETA is to IMAGE.

**Fields:**

**`videos`** &nbsp;·&nbsp; `struct × (remaining)`

Array of 322-byte VIDEOMETA elements. Element count is derived as body_size / 322; a body whose size is not a multiple of 322 is malformed and MUST be rejected.

*Element fields:*

- **`name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 64 B — Human-readable video stream name or description, null-padded to 64 bytes.
- **`device_name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 64 B — Device-name suffix used to retrieve this VIDEO via GET_VIDEO (and associated COLORT). Note the 64-byte width, wider than the 20-byte device_name suffix used by IMGMETA and LBMETA. Forms the authoritative key for this metadata entry.
- **`patient_name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 64 B — Patient name, null-padded to 64 bytes. This is PHI; implementations SHOULD treat the field as sensitive and MAY omit or redact it depending on deployment policy.
- **`patient_id`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 64 B — Patient identifier (e.g. MRN), null-padded to 64 bytes. Also PHI; same treatment guidance as patient_name.
- **`zoom_level`** &nbsp;·&nbsp; `int16` — Camera zoom level as a signed 16-bit integer. Units and reference-point semantics are device-specific; consumers SHOULD treat this as an opaque device-reported value unless they have prior calibration knowledge of the source.
- **`focal_length`** &nbsp;·&nbsp; `float64` — Camera focal length in millimeters.
- **`size`** &nbsp;·&nbsp; `uint16 × 3` — Video frame dimensions in pixels: (Ri, Rj, Rk). For a 2D video source, Rk is typically 1; three-axis sizes are preserved here for consistency with IMAGE.
- **`matrix`** &nbsp;·&nbsp; `float32 × 12` — Frame-plane orientation and origin, laid out as four float32[3] groups in row order: (TX,TY,TZ) = norm_i * pixel_size_i, (SX,SY,SZ) = norm_j * pixel_size_j, (NX,NY,NZ) = norm_k * pixel_size_k, (PX,PY,PZ) = origin (center of the frame in millimeters). Same convention as IMGMETA-adjacent IMAGE orientation, minus the scalar-type-dependent encoding variant.
- **`scalar_type`** &nbsp;·&nbsp; `uint8` — Pixel scalar type, matching VIDEO's scalar_type codes (and IMAGE's): 2=int8, 3=uint8, 4=int16, 5=uint16, 6=int32, 7=uint32, 10=float32, 11=float64. Receivers MUST accept unknown values without rejection.
- **`reserved`** &nbsp;·&nbsp; `uint8` — Reserved; senders SHOULD write 0 and receivers MUST ignore.

**Legacy notes:**

- Total body size is exactly 322 * N where N is the number of video entries. An empty VIDEOMETA body (N=0, body_size=0) is legal and means 'no video sources available'.
- VIDEOMETA was introduced in protocol v3.1 (July 2017), later than most element-based messages. The bundled reference test vector `igtl_test_data_videometa.h` carries a v3 extended header but advertises version=2 in the fixed header — senders targeting the modern protocol MUST use version=3.
- Patient name and patient ID fields carry PHI, same treatment guidance as IMGMETA: redact or reject on unencrypted transports per deployment policy. The wire format provides no redaction indicator; null-padded empty strings are the de facto redaction.
- The `device_name` inside the element is 64 bytes, unlike the 20-byte device_name used by IMGMETA / LBMETA / POINT / TRAJECTORY. Clients constructing GET_VIDEO requests by copying this field MUST truncate to the fixed-header device_name width (20 bytes) or reject entries whose device_name would not fit.
- Upstream `igtl_videometa_convert_byte_order` (pinned SHA 94244fe) carries an explicit TODO noting a possible segmentation fault when compiled with `-ftree-vectorize` (enabled by `-O3`) on 64-bit Linux. The pattern is a stack array of 12 uint32 swapped in a loop; a conformant reimplementation SHOULD byte-swap each matrix element in place without the temporary-array round-trip.
- Upstream C++ library (at pinned SHA 94244fe) iterates `nitem` elements without verifying the body is large enough. A conformant implementation MUST reject any VIDEOMETA message whose body_size is not a multiple of 322.

**See also:** [`IMGMETA`](#imgmeta), [`VIDEO`](#video), [`COLORTABLE`](#colortable), [`IMAGE`](#image)

**Spec reference:** [protocol/v3.md §"Body (VIDEOMETA)"](protocol/v3.md)

---

## Query messages — `GET_*`

### `GET_BIND`

**Type ID:** `GET_BIND` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** no

Request specific child messages from a BIND server. When body_size is 0, the server returns all available data. When body_size > 0, the body carries a selective request: a header table of type_ids (without body_sizes) plus a name table, specifying which children to include.

**Rationale:** A selective GET_BIND lets a client request only the subset of children it cares about rather than fetching the entire BIND bundle. The body reuses the BIND header/name-table format minus the body_size-per-child entries (since the client is requesting, not sending, data).

**Fields:**

**`ncmessages`** &nbsp;·&nbsp; `uint16`

Number of child message types being requested. When the outer body_size is 0, this field is absent (the entire GET_BIND body is empty, meaning 'request all').

**`type_ids`** &nbsp;·&nbsp; `fixed_string<12> × ncmessages`

Array of N × 12-byte type_id strings identifying which child message types to include in the response. Unlike the BIND header_entries, there are no body_size fields here (this is a request, not a container).

**`nametable_size`** &nbsp;·&nbsp; `uint16`

Byte length of the name table. MUST be even.

**`name_table`** &nbsp;·&nbsp; `uint8 × nametable_size`

Packed device names — ncmessages null-terminated ASCII strings, 2-byte-aligned.

**Legacy notes:**

- An empty body (body_size=0) is legal and means 'request all available children'. Receivers MUST handle this case before attempting to parse the ncmessages field.
- The selective-request layout differs from BIND's header_entries: it carries only type_id[12] per entry (no body_size uint64). This is a request/response asymmetry — the client says 'which types', the server decides the sizes.

**See also:** [`BIND`](#bind), [`STT_BIND`](#stt-bind), [`STP_BIND`](#stp-bind), [`RTS_BIND`](#rts-bind)

---

### `GET_CAPABIL`

**Type ID:** `GET_CAPABIL` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Request a CAPABILITY listing from the remote peer.

**See also:** [`CAPABILITY`](#capability)

---

### `GET_COLORT`

**Type ID:** `GET_COLORT` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Request the COLORTABLE for the device named in the header.

**See also:** [`COLORTABLE`](#colortable)

---

### `GET_IMAGE`

**Type ID:** `GET_IMAGE` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Request the current IMAGE for the device named in the header. If the header device name is empty, the server returns a default image.

**See also:** [`IMAGE`](#image), [`IMGMETA`](#imgmeta)

---

### `GET_IMGMETA`

**Type ID:** `GET_IMGMETA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Request the IMGMETA listing. If the header device name is empty, a listing for all available images is returned.

**See also:** [`IMGMETA`](#imgmeta), [`IMAGE`](#image)

---

### `GET_LBMETA`

**Type ID:** `GET_LBMETA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Request the LBMETA listing.

**See also:** [`LBMETA`](#lbmeta), [`IMAGE`](#image)

---

### `GET_NDARRAY`

**Type ID:** `GET_NDARRAY` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Request the current NDARRAY for the device named in the header.

**See also:** [`NDARRAY`](#ndarray)

---

### `GET_POINT`

**Type ID:** `GET_POINT` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Request the POINT set for the device named in the header.

**See also:** [`POINT`](#point)

---

### `GET_POLYDATA`

**Type ID:** `GET_POLYDATA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Request the current POLYDATA mesh for the device named in the header.

**See also:** [`POLYDATA`](#polydata)

---

### `GET_POSITION`

**Type ID:** `GET_POSITION` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Request the current POSITION for the device named in the header.

**See also:** [`POSITION`](#position)

---

### `GET_QTDATA`

**Type ID:** `GET_QTDATA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Request the current QTDATA for the device named in the header.

**See also:** [`QTDATA`](#qtdata)

---

### `GET_QTRANS`

**Type ID:** `GET_QTRANS` &nbsp;·&nbsp; **Introduced in:** v3 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Request the current QTRANS for the device named in the header.

**See also:** [`QTRANS`](#qtrans)

---

### `GET_SENSOR`

**Type ID:** `GET_SENSOR` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Request the current SENSOR reading for the device named in the header.

**See also:** [`SENSOR`](#sensor)

---

### `GET_STATUS`

**Type ID:** `GET_STATUS` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Request the current STATUS for the device named in the header.

**See also:** [`STATUS`](#status)

---

### `GET_STRING`

**Type ID:** `GET_STRING` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Request the current STRING for the device named in the header.

**See also:** [`STRING`](#string)

---

### `GET_TDATA`

**Type ID:** `GET_TDATA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Request the current TDATA for the device named in the header.

**See also:** [`TDATA`](#tdata)

---

### `GET_TRAJ`

**Type ID:** `GET_TRAJ` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Request the TRAJECTORY set for the device named in the header.

**See also:** [`TRAJECTORY`](#trajectory)

---

### `GET_TRANS`

**Type ID:** `GET_TRANS` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Request the current TRANSFORM for the device named in the header.

**See also:** [`TRANSFORM`](#transform)

---

### `GET_VMETA`

**Type ID:** `GET_VMETA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Request the VIDEOMETA listing.

**See also:** [`VIDEOMETA`](#videometa), [`VIDEO`](#video)

---

## Stream-start messages — `STT_*`

### `STT_BIND`

**Type ID:** `STT_BIND` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** no

Start streaming BIND messages. Prepends a uint64 time resolution to the GET_BIND selective-request layout. When the request portion has body_size 0 (after the resolution field), all available children are streamed.

**Fields:**

**`resolution`** &nbsp;·&nbsp; `uint64`

Minimum time between consecutive BIND messages, encoded in the OpenIGTLink timestamp format (upper 32 bits = seconds, lower 32 bits = fraction). 0 means 'as fast as possible'.

**`ncmessages`** &nbsp;·&nbsp; `uint16`

Number of child message types being requested. When the body after the resolution field is empty (body_size=8), this and all following fields are absent, meaning 'stream all available children'.

**`type_ids`** &nbsp;·&nbsp; `fixed_string<12> × ncmessages`

Array of N × 12-byte type_id strings identifying which child types to include.

**`nametable_size`** &nbsp;·&nbsp; `uint16`

Byte length of the name table. MUST be even.

**`name_table`** &nbsp;·&nbsp; `uint8 × nametable_size`

Packed device names — ncmessages null-terminated ASCII strings, 2-byte-aligned.

**See also:** [`BIND`](#bind), [`GET_BIND`](#get-bind), [`STP_BIND`](#stp-bind), [`RTS_BIND`](#rts-bind)

---

### `STT_IMAGE`

**Type ID:** `STT_IMAGE` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Start streaming IMAGE messages at the server's default rate.

**See also:** [`IMAGE`](#image)

---

### `STT_NDARRAY`

**Type ID:** `STT_NDARRAY` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Start streaming NDARRAY messages. Per query.md, body is empty; streaming cadence is server-determined.

**See also:** [`NDARRAY`](#ndarray)

---

### `STT_POLYDATA`

**Type ID:** `STT_POLYDATA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Start streaming POLYDATA messages at the server's default rate.

**See also:** [`POLYDATA`](#polydata)

---

### `STT_POSITION`

**Type ID:** `STT_POSITION` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Start streaming POSITION messages at the server's default rate.

**See also:** [`POSITION`](#position)

---

### `STT_QTDATA`

**Type ID:** `STT_QTDATA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 36 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Start streaming QTDATA (quaternion tracking data) messages at a specified update interval, in a named coordinate system. Identical body layout to STT_TDATA.

**Fields:**

**`resolution`** &nbsp;·&nbsp; `int32`

Minimum time between consecutive QTDATA messages, in milliseconds. 0 means 'as fast as possible'.

**`coord_name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 32 B

Name of the coordinate system for reported tracking data. Null-padded to 32 bytes. Empty string means 'server default'.

**See also:** [`QTDATA`](#qtdata), [`STP_QTDATA`](#stp-qtdata), [`RTS_QTDATA`](#rts-qtdata)

---

### `STT_QTRANS`

**Type ID:** `STT_QTRANS` &nbsp;·&nbsp; **Introduced in:** v3 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Start streaming QTRANS messages at the server's default rate.

**See also:** [`QTRANS`](#qtrans)

---

### `STT_TDATA`

**Type ID:** `STT_TDATA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 36 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Start streaming TDATA (tracking data) messages at a specified update interval, in a named coordinate system. The server responds with periodic TDATA messages until a STP_TDATA is received.

**Fields:**

**`resolution`** &nbsp;·&nbsp; `int32`

Minimum time between consecutive TDATA messages, in milliseconds. 0 means 'as fast as possible'. If 50 is specified, the maximum update rate is 20 Hz. The server MAY send slower than the requested rate but SHOULD NOT send faster.

**`coord_name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 32 B

Name of the coordinate system for reported tracking data (e.g. 'Tracker', 'Patient'). Null-padded to 32 bytes. An empty string (all nulls) means 'server default'.

**See also:** [`TDATA`](#tdata), [`STP_TDATA`](#stp-tdata), [`RTS_TDATA`](#rts-tdata)

---

### `STT_TRANS`

**Type ID:** `STT_TRANS` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Start streaming TRANSFORM messages at the server's default rate.

**See also:** [`TRANSFORM`](#transform)

---

### `STT_VIDEO`

**Type ID:** `STT_VIDEO` &nbsp;·&nbsp; **Introduced in:** v3 &nbsp;·&nbsp; **Body size:** 8 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Start streaming VIDEO frames with a specified codec and update interval. The server responds with periodic VIDEO messages until a STP_VIDEO is received.

**Fields:**

**`codec`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 4 B

Requested FourCC codec: 'I420' (uncompressed YUV), 'H264', 'VP90' (VP9), 'X265', 'O265' (OpenHEVC), 'AV10' (AV1). All four bytes significant. The server SHOULD honor the request or reject with an error status.

**`time_interval`** &nbsp;·&nbsp; `uint32`

Minimum time between consecutive VIDEO frames, in milliseconds. 0 means 'as fast as possible'.

**Legacy notes:**

- Upstream defines IGTL_STT_VIDEO_SIZE as 9, but the igtl_stt_video struct under #pragma pack(1) is 8 bytes (char[4] + uint32). The CRC function uses IGTL_STT_VIDEO_SIZE=9, computing CRC over one byte past the struct — likely a latent bug. A conformant implementation SHOULD use body_size=8.

**See also:** [`VIDEO`](#video), [`STP_VIDEO`](#stp-video), [`VIDEOMETA`](#videometa)

---

## Stream-stop messages — `STP_*`

### `STP_BIND`

**Type ID:** `STP_BIND` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Stop streaming BIND messages previously started by STT_BIND.

**See also:** [`BIND`](#bind)

---

### `STP_IMAGE`

**Type ID:** `STP_IMAGE` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Stop streaming IMAGE messages previously started by STT_IMAGE.

**See also:** [`IMAGE`](#image)

---

### `STP_NDARRAY`

**Type ID:** `STP_NDARRAY` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Stop streaming NDARRAY messages previously started by STT_NDARRAY.

**See also:** [`NDARRAY`](#ndarray)

---

### `STP_POLYDATA`

**Type ID:** `STP_POLYDATA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Stop streaming POLYDATA previously started by STT_POLYDATA.

**See also:** [`POLYDATA`](#polydata)

---

### `STP_POSITION`

**Type ID:** `STP_POSITION` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Stop streaming POSITION messages previously started by STT_POSITION.

**See also:** [`POSITION`](#position)

---

### `STP_QTDATA`

**Type ID:** `STP_QTDATA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Stop streaming QTDATA messages previously started by STT_QTDATA.

**See also:** [`QTDATA`](#qtdata)

---

### `STP_QTRANS`

**Type ID:** `STP_QTRANS` &nbsp;·&nbsp; **Introduced in:** v3 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Stop streaming QTRANS messages previously started by STT_QTRANS.

**See also:** [`QTRANS`](#qtrans)

---

### `STP_SENSOR`

**Type ID:** `STP_SENSOR` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Stop streaming SENSOR messages previously started by STT_SENSOR.

**See also:** [`SENSOR`](#sensor)

---

### `STP_TDATA`

**Type ID:** `STP_TDATA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Stop streaming TDATA messages previously started by STT_TDATA.

**See also:** [`TDATA`](#tdata)

---

### `STP_TRANS`

**Type ID:** `STP_TRANS` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Stop streaming TRANSFORM messages previously started by STT_TRANS.

**See also:** [`TRANSFORM`](#transform)

---

### `STP_VIDEO`

**Type ID:** `STP_VIDEO` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 0 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Stop streaming VIDEO frames previously started by STT_VIDEO.

**See also:** [`VIDEO`](#video)

---

## Response messages — `RTS_*`

### `RTS_BIND`

**Type ID:** `RTS_BIND` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 1 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Server's return status for a BIND query (GET/STT/STP). A single int8: 0 = success, 1 = error.

**Fields:**

**`status`** &nbsp;·&nbsp; `int8`

0 = success (the requested operation completed). 1 = error (the request was rejected or failed). Other values are reserved; receivers SHOULD treat any non-zero value as error.

**See also:** [`BIND`](#bind)

---

### `RTS_CAPABIL`

**Type ID:** `RTS_CAPABIL` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 1 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Server's return status for a CAPABILITY query (GET/STT/STP). Per Documents/Protocol/query.md, every message type has an RTS_ form for error returns. A single int8: 0 = success, 1 = error.

**Fields:**

**`status`** &nbsp;·&nbsp; `int8`

0 = success (the requested operation completed). 1 = error (request rejected or failed). Other values are reserved; receivers SHOULD treat any non-zero value as error.

**See also:** [`CAPABILITY`](#capability)

---

### `RTS_COMMAND`

**Type ID:** `RTS_COMMAND` &nbsp;·&nbsp; **Introduced in:** v3 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** yes (v2 / v3)

Reply to a COMMAND message. Reuses the COMMAND body layout — the command_id echoes the original request's ID so the sender can correlate, and the command_name field carries an error description when the command failed. The command payload in the reply typically contains the result or diagnostic output.

**Fields:**

**`command_id`** &nbsp;·&nbsp; `uint32`

Echoed command ID from the original COMMAND message. The sender uses this to match the reply to its outstanding request.

**`command_name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 128 B

On success: echoes the original command name. On error: carries an error name or description string. Null-padded to 128 bytes.

**`encoding`** &nbsp;·&nbsp; `uint16`

IANA MIBenum code for the character encoding of the `command` payload (same as COMMAND).

**`length`** &nbsp;·&nbsp; `uint32`

Byte length of the trailing `command` field.

**`command`** &nbsp;·&nbsp; `uint8 × length`

Reply payload — result data or error diagnostic, exactly `length` bytes. Typically XML. Interpreted per `encoding`.

**See also:** [`COMMAND`](#command)

---

### `RTS_IMAGE`

**Type ID:** `RTS_IMAGE` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 1 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Server's return status for a IMAGE query (GET/STT/STP). A single int8: 0 = success, 1 = error.

**Fields:**

**`status`** &nbsp;·&nbsp; `int8`

0 = success (the requested operation completed). 1 = error (the request was rejected or failed). Other values are reserved; receivers SHOULD treat any non-zero value as error.

**See also:** [`IMAGE`](#image)

---

### `RTS_IMGMETA`

**Type ID:** `RTS_IMGMETA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 1 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Server's return status for a IMGMETA query (GET/STT/STP). Per Documents/Protocol/query.md, every message type has an RTS_ form for error returns. A single int8: 0 = success, 1 = error.

**Fields:**

**`status`** &nbsp;·&nbsp; `int8`

0 = success (the requested operation completed). 1 = error (request rejected or failed). Other values are reserved; receivers SHOULD treat any non-zero value as error.

**See also:** [`IMGMETA`](#imgmeta)

---

### `RTS_LBMETA`

**Type ID:** `RTS_LBMETA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 1 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Server's return status for a LBMETA query (GET/STT/STP). Per Documents/Protocol/query.md, every message type has an RTS_ form for error returns. A single int8: 0 = success, 1 = error.

**Fields:**

**`status`** &nbsp;·&nbsp; `int8`

0 = success (the requested operation completed). 1 = error (request rejected or failed). Other values are reserved; receivers SHOULD treat any non-zero value as error.

**See also:** [`LBMETA`](#lbmeta)

---

### `RTS_NDARRAY`

**Type ID:** `RTS_NDARRAY` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 1 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Server's return status for a NDARRAY query (GET/STT/STP). Per Documents/Protocol/query.md, every message type has an RTS_ form for error returns. A single int8: 0 = success, 1 = error.

**Fields:**

**`status`** &nbsp;·&nbsp; `int8`

0 = success (the requested operation completed). 1 = error (request rejected or failed). Other values are reserved; receivers SHOULD treat any non-zero value as error.

**See also:** [`NDARRAY`](#ndarray)

---

### `RTS_POINT`

**Type ID:** `RTS_POINT` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 1 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Server's return status for a POINT query (GET/STT/STP). Per Documents/Protocol/query.md, every message type has an RTS_ form for error returns. A single int8: 0 = success, 1 = error.

**Fields:**

**`status`** &nbsp;·&nbsp; `int8`

0 = success (the requested operation completed). 1 = error (request rejected or failed). Other values are reserved; receivers SHOULD treat any non-zero value as error.

**See also:** [`POINT`](#point)

---

### `RTS_POLYDATA`

**Type ID:** `RTS_POLYDATA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 1 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Server's return status for a POLYDATA query (GET/STT/STP). A single int8: 0 = success, 1 = error.

**Fields:**

**`status`** &nbsp;·&nbsp; `int8`

0 = success (the requested operation completed). 1 = error (the request was rejected or failed). Other values are reserved; receivers SHOULD treat any non-zero value as error.

**See also:** [`POLYDATA`](#polydata)

---

### `RTS_POSITION`

**Type ID:** `RTS_POSITION` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 1 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Server's return status for a POSITION query (GET/STT/STP). A single int8: 0 = success, 1 = error.

**Fields:**

**`status`** &nbsp;·&nbsp; `int8`

0 = success (the requested operation completed). 1 = error (the request was rejected or failed). Other values are reserved; receivers SHOULD treat any non-zero value as error.

**See also:** [`POSITION`](#position)

---

### `RTS_QTDATA`

**Type ID:** `RTS_QTDATA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 1 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Server's return status for a QTDATA query (GET/STT/STP). A single int8: 0 = success, 1 = error.

**Fields:**

**`status`** &nbsp;·&nbsp; `int8`

0 = success (the requested operation completed). 1 = error (the request was rejected or failed). Other values are reserved; receivers SHOULD treat any non-zero value as error.

**See also:** [`QTDATA`](#qtdata)

---

### `RTS_QTRANS`

**Type ID:** `RTS_QTRANS` &nbsp;·&nbsp; **Introduced in:** v3 &nbsp;·&nbsp; **Body size:** 1 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Server's return status for a QTRANS query (GET/STT/STP). Per Documents/Protocol/query.md, every message type has an RTS_ form for error returns. A single int8: 0 = success, 1 = error.

**Fields:**

**`status`** &nbsp;·&nbsp; `int8`

0 = success (the requested operation completed). 1 = error (request rejected or failed). Other values are reserved; receivers SHOULD treat any non-zero value as error.

**See also:** [`QTRANS`](#qtrans)

---

### `RTS_SENSOR`

**Type ID:** `RTS_SENSOR` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 1 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Server's return status for a SENSOR query (GET/STT/STP). A single int8: 0 = success, 1 = error.

**Fields:**

**`status`** &nbsp;·&nbsp; `int8`

0 = success (the requested operation completed). 1 = error (the request was rejected or failed). Other values are reserved; receivers SHOULD treat any non-zero value as error.

**See also:** [`SENSOR`](#sensor)

---

### `RTS_STATUS`

**Type ID:** `RTS_STATUS` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 1 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Server's return status for a STATUS query (GET/STT/STP). Per Documents/Protocol/query.md, every message type has an RTS_ form for error returns. A single int8: 0 = success, 1 = error.

**Fields:**

**`status`** &nbsp;·&nbsp; `int8`

0 = success (the requested operation completed). 1 = error (request rejected or failed). Other values are reserved; receivers SHOULD treat any non-zero value as error.

**See also:** [`STATUS`](#status)

---

### `RTS_STRING`

**Type ID:** `RTS_STRING` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 1 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Server's return status for a STRING query (GET/STT/STP). Per Documents/Protocol/query.md, every message type has an RTS_ form for error returns. A single int8: 0 = success, 1 = error.

**Fields:**

**`status`** &nbsp;·&nbsp; `int8`

0 = success (the requested operation completed). 1 = error (request rejected or failed). Other values are reserved; receivers SHOULD treat any non-zero value as error.

**See also:** [`STRING`](#string)

---

### `RTS_TDATA`

**Type ID:** `RTS_TDATA` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 1 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Server's return status for a TDATA query (GET/STT/STP). A single int8: 0 = success, 1 = error.

**Fields:**

**`status`** &nbsp;·&nbsp; `int8`

0 = success (the requested operation completed). 1 = error (the request was rejected or failed). Other values are reserved; receivers SHOULD treat any non-zero value as error.

**See also:** [`TDATA`](#tdata)

---

### `RTS_TRAJ`

**Type ID:** `RTS_TRAJ` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 1 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Server's return status for a TRAJECTORY query (GET/STT/STP). Per Documents/Protocol/query.md, every message type has an RTS_ form for error returns. A single int8: 0 = success, 1 = error.

**Fields:**

**`status`** &nbsp;·&nbsp; `int8`

0 = success (the requested operation completed). 1 = error (request rejected or failed). Other values are reserved; receivers SHOULD treat any non-zero value as error.

**See also:** [`TRAJECTORY`](#trajectory)

---

### `RTS_TRANS`

**Type ID:** `RTS_TRANS` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 1 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Server's return status for a TRANSFORM query (GET/STT/STP). A single int8: 0 = success, 1 = error.

**Fields:**

**`status`** &nbsp;·&nbsp; `int8`

0 = success (the requested operation completed). 1 = error (the request was rejected or failed). Other values are reserved; receivers SHOULD treat any non-zero value as error.

**See also:** [`TRANSFORM`](#transform)

---

## Framing structures

### `EXT_HEADER`

**Type ID:** `EXT_HEADER` &nbsp;·&nbsp; **Introduced in:** v3 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** no

The v3 extended header, located at the start of the message body. Carries sizes for the metadata sections and a sender-assigned message ID. This is protocol framing — not a message type. Present only when the outer header's version field is >= 3. A codec uses this to locate the content, metadata index, and metadata body regions within the overall body.

**Fields:**

**`ext_header_size`** &nbsp;·&nbsp; `uint16`

Total size of this extended header in bytes. Currently 12 for the base fields; future revisions may extend it. Receivers MUST skip any bytes beyond offset 12 up to ext_header_size without error, to allow forward-compatible extension.

**`metadata_header_size`** &nbsp;·&nbsp; `uint16`

Byte size of the metadata index section (the key-size / encoding / value-size table). 0 means no metadata. Located at body offset (body_size - metadata_header_size - metadata_size).

**`metadata_size`** &nbsp;·&nbsp; `uint32`

Byte size of the metadata body section (the actual key/value bytes). 0 means no metadata. Located at body offset (body_size - metadata_size).

**`message_id`** &nbsp;·&nbsp; `uint32`

Sender-assigned message identifier. Opaque to the protocol — the sender uses it for internal tracking (e.g. correlating responses). 0 is a legal value meaning 'unspecified'.

**Legacy notes:**

- This schema describes the base 12-byte v3 extended header. A conformant implementation MUST read ext_header_size first and skip (ext_header_size - 12) bytes of unknown extension fields before reading the message content.
- The content region starts at body offset ext_header_size and extends to body offset (body_size - metadata_header_size - metadata_size). A codec computes: content_size = body_size - ext_header_size - metadata_header_size - metadata_size.
- The extended header's own byte order is big-endian (network order), consistent with the outer header.
- When version < 3, this structure is absent — the body starts directly with message content.

---

### `HEADER`

**Type ID:** `HEADER` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 58 bytes &nbsp;·&nbsp; **Metadata allowed:** no

The 58-byte fixed header that precedes every OpenIGTLink message on the wire. This is protocol framing, not a message type — it has no wire type_id of its own (it IS the structure that carries the type_id). Modeled here so a generic codec can parse/emit headers from the same schema infrastructure used for message bodies.

**Fields:**

**`version`** &nbsp;·&nbsp; `uint16`

Protocol version: 1, 2, or 3. Determines whether the body contains an extended header (v3) or metadata block (v2/v3). Receivers MUST reject unknown version values.

**`type`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 12 B

Message type identifier — the 12-byte ASCII string that dispatches to a body schema (e.g. 'TRANSFORM', 'IMAGE', 'GET_TDATA'). Case-sensitive. Implementations MUST treat this as fixed-width bytes and null-terminate before any use as a C string.

**`device_name`** &nbsp;·&nbsp; `fixed_string` &nbsp;·&nbsp; 20 B

Device name identifying the source or target of this message. Empty (all nulls) means 'default' or 'all'. Used by GET_* messages to specify which device to query, and by data messages to identify the sender.

**`timestamp`** &nbsp;·&nbsp; `uint64`

Message timestamp: upper 32 bits = seconds since Unix epoch (1970-01-01T00:00:00Z), lower 32 bits = fraction of second (0x00000000 = 0.0, 0x80000000 = 0.5, 0xFFFFFFFF ≈ 1.0). A value of 0 means 'unspecified'. Implementations SHOULD use NTP-synchronized clocks where available.

**`body_size`** &nbsp;·&nbsp; `uint64`

Total byte size of the body that follows this header. In v3, body_size includes the extended header and metadata block. In v1/v2, body_size is the content size plus any trailing metadata. The receiver allocates body_size bytes for the body buffer. Implementations MUST impose an implementation-defined ceiling on body_size before allocating.

**`crc`** &nbsp;·&nbsp; `uint64`

CRC-64 of the entire body (body_size bytes). Polynomial is ECMA-182 (same as xz/lzma). Implementations MUST verify CRC before acting on any body content — a mismatch is a hard rejection with no partial parsing.

**Legacy notes:**

- This schema describes protocol framing, not a message body. A codec uses it to parse/emit the header independently of the body schema dispatched by the `type` field.
- The 58-byte size is invariant across all protocol versions. The body layout varies by version, but the header does not.
- The timestamp encoding splits seconds and fraction across a single uint64. The fraction field uses all 32 bits for sub-second resolution (~0.23 nanosecond granularity). Some implementations incorrectly treat the high bit of the fraction as a sign bit — this MUST NOT be relied on.
- Multiple upstream audit findings (U-1 through U-10) stem from implementations that trust body_size without validation. A conformant implementation MUST validate body_size against message-type-specific expected ranges before any body access.

---

### `METADATA`

**Type ID:** `METADATA` &nbsp;·&nbsp; **Introduced in:** v2 &nbsp;·&nbsp; **Body size:** variable &nbsp;·&nbsp; **Metadata allowed:** no

The metadata block that carries arbitrary key/value pairs, present in v2 and v3 messages. This is protocol framing — not a message type. The block has two sections: a fixed-stride index table (8 bytes per entry) and a packed body of key/value byte runs. In v3, sizes are declared in the extended header; in v2, the metadata is located by subtracting the known content size from body_size.

**Fields:**

**`count`** &nbsp;·&nbsp; `uint16`

Number of key/value entries in the metadata block. 0 means no metadata (the rest of the block is empty). This is the first field of the metadata index section.

**`index_entries`** &nbsp;·&nbsp; `struct × count`

Index table — count × 8-byte entries, each declaring one key/value pair's sizes and encoding. The index is read first; then key/value bytes are read sequentially from the metadata body section in the same order.

*Element fields:*

- **`key_size`** &nbsp;·&nbsp; `uint16` — Byte length of the key string. MUST be > 0.
- **`value_encoding`** &nbsp;·&nbsp; `uint16` — IANA MIBenum character encoding for the value (3=US-ASCII, 106=UTF-8). Keys are always UTF-8 regardless of this field.
- **`value_size`** &nbsp;·&nbsp; `uint32` — Byte length of the value.

**`body`** &nbsp;·&nbsp; `uint8 × (remaining)`

Packed key/value byte runs, no separators or padding. For each entry i in index order: key_size[i] bytes of key (UTF-8, no null terminator), then value_size[i] bytes of value (encoded per value_encoding[i]). Total byte count MUST equal sum(key_size[i] + value_size[i]). Receivers MUST validate this sum against the declared metadata_size before accessing any key/value data.

**Legacy notes:**

- This schema describes protocol framing, not a message body. A codec uses it to parse/emit the metadata block independently of the message-type-specific content.
- The metadata index section occupies 2 + count*8 bytes. The metadata body section occupies the declared metadata_size bytes. In v3 these sizes come from the extended header; in v2 the receiver must compute them.
- Keys SHOULD be unique within a message. Behavior on duplicate keys is implementation-defined — upstream C++ keeps the last value.
- Several upstream audit findings relate to missing integer-overflow checks on key_size / value_size sums. A conformant implementation MUST verify that the sum of all key_size + value_size values equals the declared metadata body size, using overflow-safe arithmetic, before accessing any key/value data.

---

### `UNIT`

**Type ID:** `UNIT` &nbsp;·&nbsp; **Introduced in:** v1 &nbsp;·&nbsp; **Body size:** 8 bytes &nbsp;·&nbsp; **Metadata allowed:** no

Physical unit encoding — a 8-byte (uint64) packed representation of an SI unit with a metric prefix. This is a field-level encoding used inside SENSOR messages (and potentially other message types), not a standalone wire message type. Modeled here so a codec can interpret the `unit` field in SENSOR's element struct. The uint64 packs a 4-bit prefix, up to 6 base/derived unit codes (6 bits each), and 6 signed exponents (4 bits each, range [-7, 7]).

**Rationale:** Sensor data is meaningless without units. Rather than using a string ('mm/s²') which requires parsing, the protocol encodes units as a compact 8-byte integer using SI base and derived unit codes with exponents. This lets a receiver determine whether two sensor readings are compatible (same units) without string comparison, and lets it convert between prefixes (milli vs. micro) by comparing the prefix nibble.

**Fields:**

**`packed`** &nbsp;·&nbsp; `uint64`

Packed unit representation. Bit layout (MSB to LSB): bits [63:60] = prefix (4 bits, e.g. 0x0=none, 0x3=kilo, 0xB=milli), then 6 × (unit[6 bits] + exponent[4 bits]) pairs occupying bits [59:0]. Each unit code is an SI base unit (0x01=meter, 0x02=gram, 0x03=second, 0x04=ampere, 0x05=kelvin, 0x06=mole, 0x07=candela) or derived unit (0x08=radian through 0x1B=sievert). Each exponent is a 4-bit value encoded via a specific lookup (NOT two's-complement signed): 0x0..0x7 → +0..+7, 0xA..0xF → -6..-1 (specifically 0xF=-1, 0xE=-2, 0xD=-3, 0xC=-4, 0xB=-5, 0xA=-6); 0x8 and 0x9 are reserved and MUST NOT be used. Representable exponent range is therefore [-6, +7]. Unused unit/exponent slots MUST be zeroed. The packed value 0x0000000000000000 means 'dimensionless' or 'unspecified'.

**Legacy notes:**

- This schema describes a field-level encoding, not a message body. The SENSOR message carries a `unit` field (uint64) in its fixed header that is interpreted using this packing.
- The upstream igtl_unit_data struct represents the unpacked form: prefix (uint8), unit[6] (uint8 array), exp[6] (int8 array). The pack/unpack functions in igtl_unit.c convert between the 8-byte wire form and this struct.
- The 6 unit/exponent pairs allow compound units like 'meter/second²' (unit[0]=meter exp=1, unit[1]=second exp=-2, rest zeroed). The prefix applies to the entire compound unit, not to individual base units.
- Exponent encoding is a specific 16-value lookup, NOT two's-complement signed nibble: 0x0..0x7 map to +0..+7, 0xA..0xF map to -6..-1, and 0x8/0x9 are reserved. A conformant implementation MUST reject values 0x8 and 0x9 at decode time, and MUST use the lookup (not bitwise sign extension) for negative values.

**See also:** [`SENSOR`](#sensor)

---
