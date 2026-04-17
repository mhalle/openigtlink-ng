// GENERATED from spec/schemas/ext_header.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/ext_header.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> ExtHeader::pack() const {
    const std::size_t body_size = 12;
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // ext_header_size
    oigtl::runtime::byte_order::write_be_u16(out.data() + off, ext_header_size);
    off += 2;
    // metadata_header_size
    oigtl::runtime::byte_order::write_be_u16(out.data() + off, metadata_header_size);
    off += 2;
    // metadata_size
    oigtl::runtime::byte_order::write_be_u32(out.data() + off, metadata_size);
    off += 4;
    // message_id
    oigtl::runtime::byte_order::write_be_u32(out.data() + off, message_id);
    off += 4;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

ExtHeader ExtHeader::unpack(const std::uint8_t* data, std::size_t length) {
    if (length < kBodySize) {
        std::ostringstream oss;
        oss << "EXT_HEADER body requires " << kBodySize
            << " bytes, got " << length;
        throw oigtl::error::ShortBufferError(oss.str());
    }
    ExtHeader out;
    std::size_t off = 0;
    // ext_header_size
    if (off + (2) > length) { throw oigtl::error::ShortBufferError("ext_header_size: short buffer"); }
    out.ext_header_size = oigtl::runtime::byte_order::read_be_u16(data + off);
    off += 2;
    // metadata_header_size
    if (off + (2) > length) { throw oigtl::error::ShortBufferError("metadata_header_size: short buffer"); }
    out.metadata_header_size = oigtl::runtime::byte_order::read_be_u16(data + off);
    off += 2;
    // metadata_size
    if (off + (4) > length) { throw oigtl::error::ShortBufferError("metadata_size: short buffer"); }
    out.metadata_size = oigtl::runtime::byte_order::read_be_u32(data + off);
    off += 4;
    // message_id
    if (off + (4) > length) { throw oigtl::error::ShortBufferError("message_id: short buffer"); }
    out.message_id = oigtl::runtime::byte_order::read_be_u32(data + off);
    off += 4;
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
