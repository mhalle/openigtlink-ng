// GENERATED from spec/schemas/string.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/string.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/ascii.hpp"
#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> String::pack() const {
    const std::size_t body_size = (2) + (2 + value.size());
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // encoding
    oigtl::runtime::byte_order::write_be_u16(out.data() + off, encoding);
    off += 2;
    // value
    oigtl::runtime::byte_order::write_be_u16(out.data() + off, static_cast<std::uint16_t>(value.size()));
    off += 2;
    std::memcpy(out.data() + off, value.data(), value.size());
    off += value.size();
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

String String::unpack(const std::uint8_t* data, std::size_t length) {
    String out;
    std::size_t off = 0;
    // encoding
    if (off + (2) > length) { throw oigtl::error::ShortBufferError("encoding: short buffer"); }
    out.encoding = oigtl::runtime::byte_order::read_be_u16(data + off);
    off += 2;
    // value
    if (off + (2) > length) { throw oigtl::error::ShortBufferError("value: short buffer"); }
    std::uint16_t value__len = oigtl::runtime::byte_order::read_be_u16(data + off);
    off += 2;
    if (off + (value__len) > length) { throw oigtl::error::ShortBufferError("value: short buffer"); }
    out.value.assign(reinterpret_cast<const char*>(data + off), value__len);
    off += value__len;
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
