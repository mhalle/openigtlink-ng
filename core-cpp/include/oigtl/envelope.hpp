// Envelope<T> — the ergonomic unit of OpenIGTLink traffic.
//
// An Envelope carries everything a sender or receiver cares about:
// the typed message body, the wire-header fields a caller chooses
// (version, device name, timestamp, message id), and the optional
// v2/v3 metadata table.
//
// The 58-byte wire header has six fields. Four of them are
// automatic (type_id comes from `T::kTypeId`, body_size and crc are
// computed by `pack()`), leaving version/device_name/timestamp for
// the caller. These three live on Envelope.
//
// Design intent: Envelope is a plain aggregate so users write
//
//     oigtl::Envelope<Transform> env{
//         .device_name = "Probe",
//         .body = { .matrix = ... },
//     };
//
// with designated initializers. Every field has a sensible default
// so the minimum valid send is `client.send(body)` (no Envelope
// construction needed — the convenience overloads build one for
// you).
#ifndef OIGTL_ENVELOPE_HPP
#define OIGTL_ENVELOPE_HPP

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "oigtl/runtime/metadata.hpp"

namespace oigtl {

// IGTL timestamp — 32.32 fixed-point seconds since Unix epoch. The
// upper 32 bits are whole seconds; the lower 32 bits are the
// fractional second, scaled so 0x1_0000_0000 = 1s. Helpers below
// convert to/from std::chrono::system_clock::time_point.
using IgtlTimestamp = std::uint64_t;

IgtlTimestamp to_igtl_timestamp(
    std::chrono::system_clock::time_point tp);
std::chrono::system_clock::time_point
from_igtl_timestamp(IgtlTimestamp t);

// Current wall-clock as an IGTL timestamp.
IgtlTimestamp now_igtl();

// Aliases for the metadata types so callers writing against the
// ergonomic API don't need to reach into `runtime::`.
using MetadataEntry = runtime::MetadataEntry;
using Metadata = std::vector<MetadataEntry>;

// Envelope carries a typed body + the wire-header fields the caller
// controls. `T` must be one of the generated message structs (must
// expose `kTypeId`, `pack()`, and `unpack()`).
template <class T>
struct Envelope {
    // ---- outer (58-byte) header fields ----
    std::uint16_t version = 2;       // 1 = legacy v1, 2 = extended, 3 = v3
    std::string   device_name;       // null-padded to 20 bytes on wire
    IgtlTimestamp timestamp = 0;     // 0 = "fill in at send time"

    // ---- v2/v3 extended-header field ----
    std::uint32_t message_id = 0;    // optional correlation id

    // ---- typed body ----
    T body{};

    // ---- v2/v3 metadata (key → encoded bytes, preserves order) ----
    Metadata metadata;
};

// Metadata convenience — build a US-ASCII text entry.
MetadataEntry make_text_metadata(
    std::string key, std::string value);

// UTF-8 text metadata (encoding = 106).
MetadataEntry make_utf8_metadata(
    std::string key, std::string value);

// Raw-bytes metadata (encoding caller-specified).
MetadataEntry make_raw_metadata(
    std::string key,
    std::vector<std::uint8_t> value,
    std::uint16_t encoding);

// Look up a text-encoded metadata value by key. Returns nullopt if
// the key isn't present or the value isn't a text encoding.
std::optional<std::string>
metadata_text(const Metadata& meta, std::string_view key);

}  // namespace oigtl

#endif  // OIGTL_ENVELOPE_HPP
