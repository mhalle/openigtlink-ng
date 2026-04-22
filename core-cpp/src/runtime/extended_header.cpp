// v2/v3 extended header codec. Parses and emits the 12-byte
// ExtendedHeader struct that precedes the content region when
// header version ≥ 2. See extended_header.hpp for field layout
// and error semantics.

#include "oigtl/runtime/extended_header.hpp"

#include <sstream>

#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::runtime {

ExtendedHeader unpack_extended_header(const std::uint8_t* data,
                                      std::size_t length) {
    if (length < kExtendedHeaderMinSize) {
        std::ostringstream oss;
        oss << "extended header requires " << kExtendedHeaderMinSize
            << " bytes, got " << length;
        throw oigtl::error::ShortBufferError(oss.str());
    }
    ExtendedHeader h;
    h.ext_header_size      = byte_order::read_be_u16(data + 0);
    h.metadata_header_size = byte_order::read_be_u16(data + 2);
    h.metadata_size        = byte_order::read_be_u32(data + 4);
    h.message_id           = byte_order::read_be_u32(data + 8);

    if (h.ext_header_size < kExtendedHeaderMinSize) {
        std::ostringstream oss;
        oss << "extended header size " << h.ext_header_size
            << " < minimum " << kExtendedHeaderMinSize;
        throw oigtl::error::MalformedMessageError(oss.str());
    }
    if (h.ext_header_size > length) {
        std::ostringstream oss;
        oss << "extended header size " << h.ext_header_size
            << " exceeds available " << length << " bytes";
        throw oigtl::error::MalformedMessageError(oss.str());
    }
    return h;
}

void pack_extended_header(std::uint8_t* out,
                          const ExtendedHeader& header) noexcept {
    byte_order::write_be_u16(out + 0, header.ext_header_size);
    byte_order::write_be_u16(out + 2, header.metadata_header_size);
    byte_order::write_be_u32(out + 4, header.metadata_size);
    byte_order::write_be_u32(out + 8, header.message_id);
}

}  // namespace oigtl::runtime
