// v2/v3 extended header — first 12 bytes of the message body.
//
// Layout (matches spec/schemas/framing_extended_header.json and
// codec/oracle.py::_parse_extended_header):
//   off  size  field
//     0     2  ext_header_size       (uint16, BE)
//     2     2  metadata_header_size  (uint16, BE)
//     4     4  metadata_size         (uint32, BE)
//     8     4  message_id            (uint32, BE)
//   total: 12 (declared by ext_header_size; forward-compatible
//             extension may grow it — receivers MUST skip past 12).
//
// The extended header carries no message-type identification; that
// is in the outer 58-byte header. It exists only to delimit the
// body's [content | metadata_index | metadata_body] regions.
#ifndef OIGTL_RUNTIME_EXTENDED_HEADER_HPP
#define OIGTL_RUNTIME_EXTENDED_HEADER_HPP

#include <cstddef>
#include <cstdint>

namespace oigtl::runtime {

inline constexpr std::size_t kExtendedHeaderMinSize = 12;

struct ExtendedHeader {
    std::uint16_t ext_header_size = kExtendedHeaderMinSize;
    std::uint16_t metadata_header_size = 0;
    std::uint32_t metadata_size = 0;
    std::uint32_t message_id = 0;
};

// Parse the first 12 bytes (or more, per ext_header_size) of body.
// Throws ShortBufferError if length < 12, or MalformedMessageError
// if the declared ext_header_size is < 12 or > length.
ExtendedHeader unpack_extended_header(const std::uint8_t* data,
                                      std::size_t length);

// Write the 12 base fields. Caller is responsible for any reserved
// bytes between offset 12 and ext_header_size (we don't invent them).
void pack_extended_header(std::uint8_t* out,
                          const ExtendedHeader& header) noexcept;

}  // namespace oigtl::runtime

#endif  // OIGTL_RUNTIME_EXTENDED_HEADER_HPP
