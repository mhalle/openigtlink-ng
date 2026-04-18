// Framer — decouple "wire-format on the socket" from "Connection".
//
// A Framer does two jobs:
//   1. Peel one message off the front of a receive buffer.
//   2. Wrap an outbound packed message for the wire.
//
// The default v3 framer is identity for wrap (our codec already
// emits fully-framed wire bytes) and a simple
// "read 58-byte header, then body_size bytes of body" for peel.
//
// A hypothetical v4 streaming/multiplexed framer is a different
// impl of this same interface. `Connection` stays identical.
#ifndef OIGTL_TRANSPORT_FRAMER_HPP
#define OIGTL_TRANSPORT_FRAMER_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "oigtl/runtime/header.hpp"

namespace oigtl::transport {

// Optional per-message metadata emitted by a framer. v3 never sets
// this. v4 chunked framer would carry stream-id / chunk-index here.
struct FramerMetadata {
    std::string framer_name;
    // Key/value free-form — interpretation is per-framer.
    std::vector<std::pair<std::string, std::string>> attributes;
};

// One parsed wire message. `header` + `body` are a unit; together
// they reconstitute the bytes on the wire except for any framer-
// added envelope (stripped by try_parse).
struct Incoming {
    runtime::Header header;
    // Body bytes only — without the 58-byte header. Matches the
    // codec's unpack_* signature (it expects body alone).
    std::vector<std::uint8_t> body;
    std::optional<FramerMetadata> metadata;
};

class Framer {
 public:
    // Attempt to peel one message off the front of `buffer`. On
    // success, consumes the relevant prefix (erases it from the
    // vector) and returns the Incoming. On "not enough bytes yet",
    // returns nullopt and leaves `buffer` untouched.
    //
    // On malformed bytes, throws a codec `oigtl::error::*` exception
    // or a transport FramingError. Callers (the Connection
    // implementation) forward this as-is through the receive()
    // Future.
    virtual std::optional<Incoming>
    try_parse(std::vector<std::uint8_t>& buffer) = 0;

    // Wrap an outbound packed wire message (header + body already
    // laid out by the codec) for the wire. For v3, returns a copy
    // of the input — the codec output is already framed. A v4
    // framer might prepend a chunk header.
    virtual std::vector<std::uint8_t>
    frame(const std::uint8_t* wire, std::size_t length) = 0;

    virtual std::string name() const = 0;

    virtual ~Framer() = default;
};

// Default v3 framer: 58-byte header + body_size body, no envelope.
// `max_body_size` = 0 means no additional cap (body_size is still
// bounded by its 64-bit wire field). Non-zero means try_parse
// throws FramingError if the header announces body_size > this cap,
// BEFORE any body bytes are allocated — a pre-parse DoS defence.
std::unique_ptr<Framer> make_v3_framer(std::size_t max_body_size = 0);

}  // namespace oigtl::transport

#endif  // OIGTL_TRANSPORT_FRAMER_HPP
