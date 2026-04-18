// v3 framer — the default, current-protocol framer.
//
// Wire layout: 58-byte fixed header, then `header.body_size` bytes
// of body. No envelope; our codec already emits framed bytes, so
// frame() is a straight copy.
//
// try_parse consumes exactly one message off the front of the
// buffer. Returns nullopt when the buffer doesn't yet carry a full
// message (short-read, not an error).

#include "oigtl/transport/framer.hpp"

#include <cstring>

#include "oigtl/runtime/error.hpp"
#include "oigtl/runtime/header.hpp"
#include "oigtl/transport/errors.hpp"

namespace oigtl::transport {

namespace {

class V3Framer final : public Framer {
 public:
    explicit V3Framer(std::size_t max_body_size)
        : max_body_size_(max_body_size) {}

    std::optional<Incoming>
    try_parse(std::vector<std::uint8_t>& buffer) override {
        if (buffer.size() < runtime::kHeaderSize) return std::nullopt;

        // Parse the header first (throws on malformed header, e.g.
        // invalid ASCII in type_id). We don't know the body size
        // until the header parses.
        runtime::Header header = runtime::unpack_header(
            buffer.data(), buffer.size());

        // Per-policy pre-parse cap. Enforced BEFORE the body-bytes
        // availability check so a peer announcing a huge body_size
        // is rejected immediately, not after waiting for bytes.
        if (max_body_size_ > 0 && header.body_size > max_body_size_) {
            throw FramingError(
                "body_size exceeds configured max_message_size");
        }

        const std::size_t total = runtime::kHeaderSize + header.body_size;
        if (buffer.size() < total) return std::nullopt;

        Incoming inc;
        inc.header = std::move(header);
        inc.body.assign(
            buffer.begin() + runtime::kHeaderSize,
            buffer.begin() + runtime::kHeaderSize + inc.header.body_size);

        // Verify CRC now — catches wire corruption or injection. A
        // malformed CRC is a receive-side error surfaced through the
        // Future.
        runtime::verify_crc(inc.header, inc.body.data(), inc.body.size());

        // Consume the prefix. O(N) on buffer tail; callers hand us
        // their own scratch buffer and message rates are modest, so
        // that's fine.
        buffer.erase(buffer.begin(), buffer.begin() + total);

        return inc;
    }

    std::vector<std::uint8_t>
    frame(const std::uint8_t* wire, std::size_t length) override {
        return std::vector<std::uint8_t>(wire, wire + length);
    }

    std::string name() const override { return "v3"; }

 private:
    std::size_t max_body_size_ = 0;
};

}  // namespace

std::unique_ptr<Framer> make_v3_framer(std::size_t max_body_size) {
    return std::make_unique<V3Framer>(max_body_size);
}

}  // namespace oigtl::transport
