#include "oigtl/runtime/oracle.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/error.hpp"

namespace oigtl::runtime::oracle {

namespace {

// Wraps a scoped try/catch that turns any oigtl::error::ProtocolError
// into a FramingResult error string. Keeps parse_wire flat.
template <typename Fn>
bool guard(FramingResult& result, const char* phase, Fn&& fn) {
    try {
        fn();
        return true;
    } catch (const oigtl::error::ProtocolError& exc) {
        std::ostringstream oss;
        oss << phase << ": " << exc.what();
        result.error = oss.str();
        return false;
    }
}

}  // namespace

FramingResult parse_wire(const std::uint8_t* data,
                         std::size_t length,
                         bool check_crc) {
    FramingResult result;

    // --- Header ---
    if (!guard(result, "header parse", [&] {
        result.header = unpack_header(data, length);
    })) return result;

    // --- Body bounds ---
    const std::size_t body_start = kHeaderSize;
    const std::size_t body_end = kHeaderSize + result.header.body_size;
    if (length < body_end) {
        std::ostringstream oss;
        oss << "truncated: header declares body_size="
            << result.header.body_size << ", but only "
            << (length - kHeaderSize) << " body bytes available";
        result.error = oss.str();
        return result;
    }
    const std::uint8_t* body_ptr = data + body_start;
    const std::size_t body_len = result.header.body_size;

    // --- CRC ---
    if (check_crc) {
        if (!guard(result, "crc verify", [&] {
            verify_crc(result.header, body_ptr, body_len);
        })) return result;
    }

    // --- v1: body is content, no framing ---
    if (result.header.version < 2) {
        result.content_bytes.assign(body_ptr, body_ptr + body_len);
        result.ok = true;
        return result;
    }

    // --- v2+: parse extended header ---
    ExtendedHeader eh;
    if (!guard(result, "extended header", [&] {
        eh = unpack_extended_header(body_ptr, body_len);
    })) return result;
    result.extended_header = eh;
    result.ext_header_bytes.assign(body_ptr, body_ptr + eh.ext_header_size);

    // --- Slice content / metadata regions ---
    const std::size_t metadata_total =
        static_cast<std::size_t>(eh.metadata_header_size)
        + static_cast<std::size_t>(eh.metadata_size);
    const std::size_t content_start = eh.ext_header_size;
    if (metadata_total > body_len
        || content_start + metadata_total > body_len) {
        std::ostringstream oss;
        oss << "framing inconsistent: ext_header_size="
            << eh.ext_header_size << ", metadata_total="
            << metadata_total << ", body_size=" << body_len;
        result.error = oss.str();
        return result;
    }
    const std::size_t content_end = body_len - metadata_total;
    result.content_bytes.assign(body_ptr + content_start,
                                body_ptr + content_end);
    result.metadata_bytes.assign(body_ptr + content_end,
                                 body_ptr + body_len);

    // --- Parse metadata ---
    if (!guard(result, "metadata parse", [&] {
        result.metadata = unpack_metadata(
            result.metadata_bytes.data(),
            result.metadata_bytes.size(),
            eh.metadata_header_size,
            eh.metadata_size);
    })) return result;

    result.ok = true;
    return result;
}


// ---------------------------------------------------------------------------
// Type-erased verify_wire_bytes
// ---------------------------------------------------------------------------

VerifyResult verify_wire_bytes(const std::uint8_t* data,
                               std::size_t length,
                               const Registry& registry,
                               bool check_crc) {
    VerifyResult out;

    auto framing = parse_wire(data, length, check_crc);
    out.header = framing.header;
    out.extended_header = framing.extended_header;
    out.metadata = framing.metadata;
    if (!framing.ok) {
        out.error = framing.error;
        return out;
    }

    auto fn = registry.lookup(framing.header.type_id);
    if (fn == nullptr) {
        std::ostringstream oss;
        oss << "no codec registered for type_id='"
            << framing.header.type_id << "'";
        out.error = oss.str();
        return out;
    }

    std::vector<std::uint8_t> content_repacked;
    try {
        content_repacked = fn(framing.content_bytes.data(),
                              framing.content_bytes.size());
    } catch (const oigtl::error::ProtocolError& exc) {
        std::ostringstream oss;
        oss << "round-trip failed: " << exc.what();
        out.error = oss.str();
        return out;
    }

    if (content_repacked.size() != framing.content_bytes.size()) {
        std::ostringstream oss;
        oss << "content round-trip length mismatch (orig "
            << framing.content_bytes.size() << "B, repacked "
            << content_repacked.size() << "B)";
        out.error = oss.str();
        return out;
    }
    if (std::memcmp(content_repacked.data(),
                    framing.content_bytes.data(),
                    framing.content_bytes.size()) != 0) {
        // Same length, different bytes. Canonical-form check: if a
        // second round-trip is stable, the codec has reached a fixed
        // point and the input simply wasn't in canonical form. Covers
        // any normalization the per-message unpack/pack applies —
        // pertinent for the rare case where a cross-language mutated
        // input lands on bytes the Python/TS codecs also normalize.
        // Matches the canonical-form logic in corpus-tools oracle.py
        // and core-ts runtime/oracle.ts.
        std::vector<std::uint8_t> second_repack;
        try {
            second_repack = fn(content_repacked.data(),
                               content_repacked.size());
        } catch (const oigtl::error::ProtocolError& exc) {
            std::ostringstream oss;
            oss << "canonicalization probe failed: " << exc.what();
            out.error = oss.str();
            return out;
        }
        if (second_repack.size() != content_repacked.size()
            || std::memcmp(second_repack.data(),
                           content_repacked.data(),
                           content_repacked.size()) != 0) {
            std::ostringstream oss;
            oss << "content round-trip unstable (orig "
                << framing.content_bytes.size() << "B, repacked "
                << content_repacked.size() << "B, second repack differs)";
            out.error = oss.str();
            return out;
        }
        // Accepted as canonical-form. The reassembly check below
        // would fail against data+kHeaderSize because content bytes
        // changed — skip it in this path (the ext_header/metadata
        // regions are raw byte slices and don't need verification).
        out.round_trip_ok = true;
        out.ok = true;
        return out;
    }

    // Strict byte-equal path — body reassembly must match exactly.
    // Preserves raw ext_header / metadata bytes (forward-compat
    // reserved bytes are not expressible via canonical pack).
    std::vector<std::uint8_t> body;
    body.reserve(framing.ext_header_bytes.size()
                 + content_repacked.size()
                 + framing.metadata_bytes.size());
    body.insert(body.end(),
                framing.ext_header_bytes.begin(),
                framing.ext_header_bytes.end());
    body.insert(body.end(),
                content_repacked.begin(),
                content_repacked.end());
    body.insert(body.end(),
                framing.metadata_bytes.begin(),
                framing.metadata_bytes.end());
    if (body.size() != framing.header.body_size
        || std::memcmp(body.data(), data + kHeaderSize, body.size()) != 0) {
        out.error = "body reassembly mismatch";
        return out;
    }

    out.round_trip_ok = true;
    out.ok = true;
    return out;
}

}  // namespace oigtl::runtime::oracle
