// Conformance oracle — slice raw wire bytes into framed regions.
//
// Mirrors codec/oracle.py::verify_wire_bytes for the framing-aware
// half of the pipeline. The schema-driven content unpack/repack is
// not part of this header (it requires per-message type dispatch,
// which the C++ port handles either by template — for tests — or by
// a runtime registry — Phase 6).
//
// Typical use:
//
//     auto framing = oracle::parse_wire(data, length);
//     if (!framing.ok) { ...handle framing.error... }
//     // framing.content_bytes — pass to MyMessage::unpack(...)
//     // framing.metadata     — already-parsed metadata entries
//     // framing.{ext_header_bytes, metadata_bytes} — preserve to
//     //   reassemble byte-exact for round-trip verification
//
// For v1 messages, ext_header_bytes is empty, metadata is empty, and
// content_bytes is the entire body.
#ifndef OIGTL_RUNTIME_ORACLE_HPP
#define OIGTL_RUNTIME_ORACLE_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "oigtl/runtime/dispatch.hpp"
#include "oigtl/runtime/extended_header.hpp"
#include "oigtl/runtime/header.hpp"
#include "oigtl/runtime/metadata.hpp"

namespace oigtl::runtime::oracle {

struct FramingResult {
    bool ok = false;
    std::string error;

    Header header;
    std::optional<ExtendedHeader> extended_header;

    // Raw region slices into the original buffer's body. Together
    // they reconstruct body_bytes exactly:
    //   body_bytes == ext_header_bytes + content_bytes + metadata_bytes
    std::vector<std::uint8_t> ext_header_bytes;
    std::vector<std::uint8_t> content_bytes;
    std::vector<std::uint8_t> metadata_bytes;

    std::vector<MetadataEntry> metadata;
};

// Parse a complete wire message. Verifies the 58-byte header and
// (if check_crc) the body CRC. If header.version >= 2, parses the
// extended header and slices the body into its three regions and
// decodes the metadata index + body.
//
// On failure, ok=false and error carries a human-readable
// description; partial results (header, extended_header, content
// bytes) may still be populated up to the point of failure.
FramingResult parse_wire(const std::uint8_t* data,
                         std::size_t length,
                         bool check_crc = true);


// ---------------------------------------------------------------------------
// Type-erased verification — combines parse_wire with a Registry
// dispatch so callers don't need to know the message type at compile
// time. Mirrors codec/oracle.py::verify_wire_bytes.
// ---------------------------------------------------------------------------

struct VerifyResult {
    bool ok = false;
    std::string error;

    Header header;
    std::optional<ExtendedHeader> extended_header;
    std::vector<MetadataEntry> metadata;

    // True iff the round-trip via the registered codec produced
    // the exact same content bytes (and the full body reassembles
    // byte-for-byte).
    bool round_trip_ok = false;
};

// Run the full oracle pipeline:
// 1. parse_wire (header → CRC → framing → extended header →
//    metadata).
// 2. Look up content codec in *registry*.
// 3. Round-trip the content bytes through the codec.
// 4. Reassemble the body using preserved ext_header / metadata bytes
//    and verify byte-equality with the input.
//
// On any failure, ok=false and error is populated; the partial
// header / framing fields are still populated up to the failure
// point.
VerifyResult verify_wire_bytes(const std::uint8_t* data,
                               std::size_t length,
                               const Registry& registry,
                               bool check_crc = true);

}  // namespace oigtl::runtime::oracle

#endif  // OIGTL_RUNTIME_ORACLE_HPP
