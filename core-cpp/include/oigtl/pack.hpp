// pack(envelope) / unpack<T>(incoming) — the ergonomic bridge
// between typed `Envelope<T>` and the runtime codec.
//
// v1 bodies are just `T::pack()`'s output.
// v2/v3 bodies are [ ext_header | content | metadata ].
//
// These helpers let callers skip the framing plumbing entirely:
//
//     auto wire = oigtl::pack(env);     // ready to send
//     auto env  = oigtl::unpack<T>(inc); // throws on type mismatch
//
// Header-only — the templates need to see the message type. Each
// generated message exposes `static constexpr const char* kTypeId`,
// `std::vector<uint8_t> pack() const`, and a static
// `T unpack(const uint8_t*, size_t)` that together let these
// helpers stay generic.
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

// Thrown by `unpack<T>()` when the incoming message's type_id
// doesn't match `T::kTypeId`. Distinct from the codec's
// ProtocolError hierarchy because it's a caller-logic issue, not
// a wire-format one.
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

// Pack an Envelope to a fully-framed wire message (58-byte header +
// body). The body layout depends on env.version:
//   - v1:      content = T::pack() output; body = content.
//   - v2, v3:  body = [12-byte ext header][content][metadata].
//
// If env.timestamp is zero, `now_igtl()` is substituted.
template <class T>
std::vector<std::uint8_t> pack(const Envelope<T>& env) {
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

// Unpack an Incoming into a typed Envelope<T>. Throws
// MessageTypeMismatch if `inc.header.type_id != T::kTypeId`;
// delegates to T::unpack for body content errors.
template <class T>
Envelope<T> unpack(const transport::Incoming& inc) {
    if (inc.header.type_id != T::kTypeId) {
        throw MessageTypeMismatch(T::kTypeId, inc.header.type_id);
    }

    Envelope<T> env;
    env.version = inc.header.version;
    env.device_name = inc.header.device_name;
    env.timestamp = inc.header.timestamp;

    if (inc.header.version == 1) {
        env.body = T::unpack(inc.body.data(), inc.body.size());
        return env;
    }

    // v2/v3: peel the extended header, then split body into
    // (content | metadata) using the declared sizes.
    if (inc.body.size() < runtime::kExtendedHeaderMinSize) {
        throw error::ShortBufferError(
            "v2/v3 body too small for extended header");
    }
    auto ext = runtime::unpack_extended_header(
        inc.body.data(), inc.body.size());
    env.message_id = ext.message_id;

    const std::size_t hdr_size = ext.ext_header_size;
    const std::size_t meta_total =
        static_cast<std::size_t>(ext.metadata_header_size) +
        static_cast<std::size_t>(ext.metadata_size);
    if (inc.body.size() < hdr_size + meta_total) {
        throw error::MalformedMessageError(
            "declared metadata exceeds body length");
    }
    const std::size_t content_size =
        inc.body.size() - hdr_size - meta_total;

    // Decode content.
    env.body = T::unpack(inc.body.data() + hdr_size, content_size);

    // Decode metadata (if any).
    if (meta_total > 0) {
        env.metadata = runtime::unpack_metadata(
            inc.body.data() + hdr_size + content_size,
            meta_total,
            ext.metadata_header_size,
            ext.metadata_size);
    }

    return env;
}

}  // namespace oigtl

#endif  // OIGTL_PACK_HPP
