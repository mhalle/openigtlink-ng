// v2/v3 metadata block — sits at the tail of the message body.
//
// Layout:
//
//   metadata = [ index | body ]
//
//   index (metadata_header_size bytes):
//     uint16 count
//     repeat count times:
//       uint16 key_size
//       uint16 value_encoding   (IANA MIBenum: 3=US-ASCII, 106=UTF-8, ...)
//       uint32 value_size
//
//   body (metadata_size bytes):
//     for each entry, in order:
//       key   (key_size bytes, UTF-8)
//       value (value_size bytes, encoding per index)
//
// The two sizes (metadata_header_size, metadata_size) are
// declared in the extended header; the metadata block occupies the
// final (metadata_header_size + metadata_size) bytes of the body.
//
// Mirrors codec/oracle.py::_parse_metadata. Duplicate keys preserve
// insertion order, so we expose this as a vector<MetadataEntry>
// rather than a map.
#ifndef OIGTL_RUNTIME_METADATA_HPP
#define OIGTL_RUNTIME_METADATA_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::runtime {

struct MetadataEntry {
    std::string key;
    std::uint16_t value_encoding = 3;       // IANA MIBenum, 3 = US-ASCII
    std::vector<std::uint8_t> value;
};

// Parse the metadata region. *region* is exactly the
// (metadata_header_size + metadata_size) trailing bytes of the body.
// Throws MalformedMessageError on size or count inconsistencies.
std::vector<MetadataEntry> unpack_metadata(
    const std::uint8_t* region,
    std::size_t region_length,
    std::uint16_t metadata_header_size,
    std::uint32_t metadata_size);

// Re-pack a metadata vector. Returns
// (index_bytes, body_bytes) — the caller concatenates them and
// stores the sizes in the extended header.
struct PackedMetadata {
    std::vector<std::uint8_t> index_bytes;
    std::vector<std::uint8_t> body_bytes;
};

PackedMetadata pack_metadata(const std::vector<MetadataEntry>& entries);

}  // namespace oigtl::runtime

#endif  // OIGTL_RUNTIME_METADATA_HPP
