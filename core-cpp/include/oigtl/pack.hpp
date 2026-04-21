// Pure wire codec — the typed, compile-time-known decode/encode API.
//
// Sits above the header codec (runtime/header.hpp) and below every
// transport (client, server, gateway). Mirrors the Python
// `oigtl.codec` and TypeScript `@openigtlink/core/codec` modules —
// same function vocabulary (unpack_header / unpack_message /
// unpack_envelope / pack_header / pack_envelope), adapted to C++'s
// compile-time typed `Envelope<T>`.
//
// The four-step pattern
// ---------------------
//
// Decoding one wire message follows the same four steps regardless
// of transport:
//
//   1. read_header_bytes  — I/O. Read exactly kHeaderSize bytes.
//   2. unpack_header      — pure. Parse them into a Header.
//   3. read_message_bytes — I/O. Read header.body_size more bytes.
//   4. unpack_message     — pure. Parse (header, body) → Envelope<T>.
//
// Streaming callers (TCP via the framer) use the two-step pair.
// Non-streaming callers (a complete WS binary frame, an MQTT
// payload, a file slice, a unit-test fixture) use unpack_envelope
// on the whole buffer.
//
// Because C++ is compile-time typed, the caller picks T. If the
// wire's type_id doesn't match T::kTypeId, MessageTypeMismatch is
// raised — there is no runtime dispatch path here. For runtime
// dispatch (the Python-equivalent "whatever type this is, return
// it typed-erased") see the Registry in runtime/dispatch.hpp.
//
// Round-trip requirement: for any Envelope<T> produced by
// unpack_envelope<T>(wire), pack_envelope(env) returns wire bytes
// that byte-compare equal to the original `wire`.
//
// Header-only — the templates need to see the concrete T. Each
// generated message exposes `static constexpr const char* kTypeId`,
// `std::vector<uint8_t> pack() const`, and a static
// `T unpack(const uint8_t*, size_t)` that together let these
// helpers stay generic over every message type.
#ifndef OIGTL_PACK_HPP
#define OIGTL_PACK_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "oigtl/envelope.hpp"
#include "oigtl/runtime/error.hpp"
#include "oigtl/runtime/extended_header.hpp"
#include "oigtl/runtime/header.hpp"
#include "oigtl/runtime/metadata.hpp"
#include "oigtl/transport/framer.hpp"  // for transport::Incoming

namespace oigtl {

// Thrown by unpack_envelope<T>() / unpack_message<T>() when the
// incoming message's type_id doesn't match T::kTypeId. Distinct
// from the codec's ProtocolError hierarchy because it's a
// caller-logic issue, not a wire-format one.
class MessageTypeMismatch : public std::runtime_error {
 public:
    MessageTypeMismatch(std::string_view expected, std::string_view got)
        : std::runtime_error(
              "expected " + std::string(expected) +
              ", got " + std::string(got)),
          expected_(expected), got_(got) {}
    const std::string& expected() const noexcept { return expected_; }
    const std::string& got() const noexcept { return got_; }

 private:
    std::string expected_;
    std::string got_;
};

namespace detail {

// Given a decoded header and its body bytes, decompose v1 / v2+
// framing and call T::unpack on the content slice. Shared between
// the Incoming overload and the raw-bytes overload.
template <class T>
Envelope<T> unpack_message_impl(const runtime::Header& header,
                                const std::uint8_t* body,
                                std::size_t body_length) {
    if (header.type_id != T::kTypeId) {
        throw MessageTypeMismatch(T::kTypeId, header.type_id);
    }
    Envelope<T> env;
    env.version = header.version;
    env.device_name = header.device_name;
    env.timestamp = header.timestamp;

    if (header.version == 1) {
        env.body = T::unpack(body, body_length);
        return env;
    }

    // v2/v3: body = [12-byte ext header][content][metadata].
    if (body_length < runtime::kExtendedHeaderMinSize) {
        throw error::ShortBufferError(
            "v2/v3 body too small for extended header");
    }
    auto ext = runtime::unpack_extended_header(body, body_length);
    env.message_id = ext.message_id;

    const std::size_t hdr_size = ext.ext_header_size;
    const std::size_t meta_total =
        static_cast<std::size_t>(ext.metadata_header_size) +
        static_cast<std::size_t>(ext.metadata_size);
    if (body_length < hdr_size + meta_total) {
        throw error::MalformedMessageError(
            "declared metadata exceeds body length");
    }
    const std::size_t content_size = body_length - hdr_size - meta_total;

    env.body = T::unpack(body + hdr_size, content_size);
    if (meta_total > 0) {
        env.metadata = runtime::unpack_metadata(
            body + hdr_size + content_size,
            meta_total,
            ext.metadata_header_size,
            ext.metadata_size);
    }
    return env;
}

}  // namespace detail

// ---------------------------------------------------------------------------
// pack_envelope — serialize Envelope<T> to a fully-framed wire
// message (58-byte header + body).
// ---------------------------------------------------------------------------

// If env.timestamp is zero, now_igtl() is substituted.
template <class T>
std::vector<std::uint8_t> pack_envelope(const Envelope<T>& env) {
    auto content = env.body.pack();
    const IgtlTimestamp ts = env.timestamp != 0
        ? env.timestamp
        : now_igtl();

    std::vector<std::uint8_t> body;
    if (env.version == 1) {
        body = std::move(content);
    } else {
        // v2/v3: prepend 12-byte extended header, append metadata.
        auto packed_meta = runtime::pack_metadata(env.metadata);

        runtime::ExtendedHeader ext{};
        ext.ext_header_size = runtime::kExtendedHeaderMinSize;
        ext.metadata_header_size = static_cast<std::uint16_t>(
            packed_meta.index_bytes.size());
        ext.metadata_size = static_cast<std::uint32_t>(
            packed_meta.body_bytes.size());
        ext.message_id = env.message_id;

        body.resize(runtime::kExtendedHeaderMinSize);
        runtime::pack_extended_header(body.data(), ext);
        body.insert(body.end(), content.begin(), content.end());
        body.insert(body.end(),
                    packed_meta.index_bytes.begin(),
                    packed_meta.index_bytes.end());
        body.insert(body.end(),
                    packed_meta.body_bytes.begin(),
                    packed_meta.body_bytes.end());
    }

    std::array<std::uint8_t, runtime::kHeaderSize> hdr{};
    runtime::pack_header(hdr, env.version, T::kTypeId, env.device_name,
                         ts, body.data(), body.size());

    std::vector<std::uint8_t> wire;
    wire.reserve(hdr.size() + body.size());
    wire.insert(wire.end(), hdr.begin(), hdr.end());
    wire.insert(wire.end(), body.begin(), body.end());
    return wire;
}

// ---------------------------------------------------------------------------
// unpack_message — step 4 of the four-step pattern (streaming path).
// Takes the already-parsed header plus the freshly-read body bytes.
// ---------------------------------------------------------------------------

template <class T>
Envelope<T> unpack_message(const runtime::Header& header,
                           const std::uint8_t* body,
                           std::size_t body_length) {
    if (body_length != header.body_size) {
        throw error::MalformedMessageError(
            "body length does not match header.body_size");
    }
    return detail::unpack_message_impl<T>(header, body, body_length);
}

template <class T>
Envelope<T> unpack_message(const runtime::Header& header,
                           const std::vector<std::uint8_t>& body) {
    return unpack_message<T>(header, body.data(), body.size());
}

// ---------------------------------------------------------------------------
// unpack_envelope — convenience for callers holding the complete
// wire message in memory (MQTT payload, file slice, test fixture,
// full WS binary frame). Wraps unpack_header + unpack_message.
// ---------------------------------------------------------------------------

template <class T>
Envelope<T> unpack_envelope(const std::uint8_t* wire,
                            std::size_t wire_length) {
    if (wire_length < runtime::kHeaderSize) {
        throw error::ShortBufferError(
            "wire shorter than header");
    }
    auto header = runtime::unpack_header(wire, runtime::kHeaderSize);
    const std::size_t expected =
        runtime::kHeaderSize + header.body_size;
    if (wire_length < expected) {
        throw error::ShortBufferError(
            "wire truncated: body_size declares more than available");
    }
    if (wire_length > expected) {
        throw error::MalformedMessageError(
            "wire has trailing bytes beyond declared body_size");
    }
    const std::uint8_t* body = wire + runtime::kHeaderSize;
    runtime::verify_crc(header, body, header.body_size);
    return detail::unpack_message_impl<T>(header, body, header.body_size);
}

template <class T>
Envelope<T> unpack_envelope(const std::vector<std::uint8_t>& wire) {
    return unpack_envelope<T>(wire.data(), wire.size());
}

// ---------------------------------------------------------------------------
// Incoming-based form — the transport layer's native shape. A
// framer yields an Incoming; this helper threads it through the
// same pure codec.
// ---------------------------------------------------------------------------

template <class T>
Envelope<T> unpack_envelope(const transport::Incoming& inc) {
    return detail::unpack_message_impl<T>(
        inc.header, inc.body.data(), inc.body.size());
}

}  // namespace oigtl

#endif  // OIGTL_PACK_HPP
