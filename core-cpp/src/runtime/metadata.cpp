// v2/v3 metadata region codec. Packs and parses the
// [metadata_header][metadata_body] section that sits after the
// content region when the extended header advertises a non-zero
// metadata size. See metadata.hpp for entry layout and encoding
// rules.

#include "oigtl/runtime/metadata.hpp"

#include <cstring>
#include <limits>
#include <sstream>

#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::runtime {

namespace {

constexpr std::size_t kIndexEntrySize = 8;  // u16 + u16 + u32
constexpr std::size_t kCountFieldSize = 2;

}  // namespace

std::vector<MetadataEntry> unpack_metadata(
    const std::uint8_t* region,
    std::size_t region_length,
    std::uint16_t metadata_header_size,
    std::uint32_t metadata_size) {

    if (metadata_header_size == 0 && metadata_size == 0) {
        return {};
    }

    const std::size_t declared_total =
        static_cast<std::size_t>(metadata_header_size)
        + static_cast<std::size_t>(metadata_size);
    if (region_length < declared_total) {
        std::ostringstream oss;
        oss << "metadata region shorter than declared: have "
            << region_length << ", need " << declared_total;
        throw oigtl::error::MalformedMessageError(oss.str());
    }
    if (metadata_header_size < kCountFieldSize) {
        std::ostringstream oss;
        oss << "metadata_header_size " << metadata_header_size
            << " < " << kCountFieldSize << " (count field)";
        throw oigtl::error::MalformedMessageError(oss.str());
    }

    const std::uint16_t count = byte_order::read_be_u16(region + 0);
    const std::size_t expected_header_bytes =
        kCountFieldSize + static_cast<std::size_t>(count) * kIndexEntrySize;
    if (metadata_header_size < expected_header_bytes) {
        std::ostringstream oss;
        oss << "metadata_header_size " << metadata_header_size
            << " too small for " << count << " entries (need "
            << expected_header_bytes << ")";
        throw oigtl::error::MalformedMessageError(oss.str());
    }

    // Pass 1: decode the index entries.
    struct IndexEntry {
        std::uint16_t key_size;
        std::uint16_t value_encoding;
        std::uint32_t value_size;
    };
    std::vector<IndexEntry> index;
    index.reserve(count);
    std::size_t off = kCountFieldSize;
    for (std::uint16_t i = 0; i < count; ++i) {
        IndexEntry e;
        e.key_size       = byte_order::read_be_u16(region + off + 0);
        e.value_encoding = byte_order::read_be_u16(region + off + 2);
        e.value_size     = byte_order::read_be_u32(region + off + 4);
        index.push_back(e);
        off += kIndexEntrySize;
    }

    // Pass 2: decode the body, starting at metadata_header_size.
    std::vector<MetadataEntry> result;
    result.reserve(count);
    std::size_t body_off = metadata_header_size;
    for (const auto& e : index) {
        const std::size_t need =
            static_cast<std::size_t>(e.key_size)
            + static_cast<std::size_t>(e.value_size);
        if (body_off + need > declared_total) {
            std::ostringstream oss;
            oss << "metadata entry overruns region: body_off "
                << body_off << " + " << need << " > "
                << declared_total;
            throw oigtl::error::MalformedMessageError(oss.str());
        }
        MetadataEntry m;
        m.key.assign(reinterpret_cast<const char*>(region + body_off),
                     e.key_size);
        body_off += e.key_size;
        m.value_encoding = e.value_encoding;
        m.value.assign(region + body_off,
                       region + body_off + e.value_size);
        body_off += e.value_size;
        result.push_back(std::move(m));
    }

    return result;
}

PackedMetadata pack_metadata(const std::vector<MetadataEntry>& entries) {
    PackedMetadata out;

    // Size validation. Two layers of wire-format limits to enforce:
    //
    // 1. Per-entry: each key_size is u16 and each value_size is u32.
    //    Oversize entries silently truncated would hand the memcpy
    //    loop a mismatched body buffer — heap overflow. Reject at
    //    the source.
    //
    // 2. Aggregate: the extended-header fields metadata_header_size
    //    (u16) and metadata_size (u32) encode the *packed* totals.
    //    The count cap alone is insufficient — with 8192 entries,
    //    index_bytes is 2 + 8192*8 = 65538 bytes, above u16, and
    //    the caller's cast truncates it to 2. The cap that keeps
    //    index_bytes in u16 is therefore count <= 8191, not 65535.
    //    Same reasoning for body: accumulate in u64 and reject when
    //    the sum exceeds u32 even if each individual value fits.
    constexpr std::size_t kMaxCount =
        (std::numeric_limits<std::uint16_t>::max() - kCountFieldSize)
        / kIndexEntrySize;  // 8191
    if (entries.size() > kMaxCount) {
        throw oigtl::error::MalformedMessageError(
            "metadata: too many entries (max 8191 — index must fit u16)");
    }
    std::uint64_t body_total = 0;
    for (const auto& m : entries) {
        if (m.key.size() > std::numeric_limits<std::uint16_t>::max()) {
            throw oigtl::error::MalformedMessageError(
                "metadata: key size exceeds u16 (max 65535 bytes)");
        }
        if (m.value.size() > std::numeric_limits<std::uint32_t>::max()) {
            throw oigtl::error::MalformedMessageError(
                "metadata: value size exceeds u32 (max 4 GiB)");
        }
        body_total += static_cast<std::uint64_t>(m.key.size())
                    + static_cast<std::uint64_t>(m.value.size());
        if (body_total > std::numeric_limits<std::uint32_t>::max()) {
            throw oigtl::error::MalformedMessageError(
                "metadata: aggregate body size exceeds u32 (max 4 GiB)");
        }
    }

    // Always emit the 2-byte count field, even for empty metadata.
    // This matches the wire layout: a v2/v3 message with no metadata
    // typically carries metadata_header_size=2, metadata_size=0
    // (just the count=0 field). Returning an empty index would
    // disagree with that on round-trip.
    const std::uint16_t count = static_cast<std::uint16_t>(entries.size());
    out.index_bytes.resize(kCountFieldSize + count * kIndexEntrySize);
    byte_order::write_be_u16(out.index_bytes.data() + 0, count);

    std::size_t body_size = 0;
    std::size_t off = kCountFieldSize;
    for (const auto& m : entries) {
        const std::uint16_t key_size = static_cast<std::uint16_t>(m.key.size());
        const std::uint32_t value_size =
            static_cast<std::uint32_t>(m.value.size());
        byte_order::write_be_u16(out.index_bytes.data() + off + 0, key_size);
        byte_order::write_be_u16(out.index_bytes.data() + off + 2,
                                 m.value_encoding);
        byte_order::write_be_u32(out.index_bytes.data() + off + 4, value_size);
        off += kIndexEntrySize;
        body_size += key_size + value_size;
    }

    out.body_bytes.resize(body_size);
    std::size_t boff = 0;
    for (const auto& m : entries) {
        std::memcpy(out.body_bytes.data() + boff, m.key.data(), m.key.size());
        boff += m.key.size();
        std::memcpy(out.body_bytes.data() + boff,
                    m.value.data(), m.value.size());
        boff += m.value.size();
    }

    return out;
}

}  // namespace oigtl::runtime
