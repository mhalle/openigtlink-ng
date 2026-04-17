// GENERATED from spec/schemas/status.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/status.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/ascii.hpp"
#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Status::pack() const {
    const std::size_t body_size = (2) + (8) + (20) + (status_message.size() + 1);
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // code
    oigtl::runtime::byte_order::write_be_u16(out.data() + off, code);
    off += 2;
    // subcode
    oigtl::runtime::byte_order::write_be_i64(out.data() + off, subcode);
    off += 8;
    // error_name
    {
        constexpr std::size_t n = 20;
        std::size_t copy_len = error_name.size() < n ? error_name.size() : n;
        std::memcpy(out.data() + off, error_name.data(), copy_len);
        if (copy_len < n) {
            std::memset(out.data() + off + copy_len, 0, n - copy_len);
        }
        off += 20;
    }
    // status_message
    std::memcpy(out.data() + off, status_message.data(), status_message.size());
    off += status_message.size();
    out[off] = 0;
    off += 1;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Status Status::unpack(const std::uint8_t* data, std::size_t length) {
    Status out;
    std::size_t off = 0;
    // code
    if (off + (2) > length) { throw oigtl::error::ShortBufferError("code: short buffer"); }
    out.code = oigtl::runtime::byte_order::read_be_u16(data + off);
    off += 2;
    // subcode
    if (off + (8) > length) { throw oigtl::error::ShortBufferError("subcode: short buffer"); }
    out.subcode = oigtl::runtime::byte_order::read_be_i64(data + off);
    off += 8;
    // error_name
    if (off + (20) > length) { throw oigtl::error::ShortBufferError("error_name: short buffer"); }
    {
        constexpr std::size_t n = 20;
        const std::size_t len = oigtl::runtime::ascii::null_padded_length(data + off, n, "error_name");
        out.error_name.assign(reinterpret_cast<const char*>(data + off), len);
        off += 20;
    }
    // status_message
    {
        std::size_t end = length;
        if (end > off && data[end - 1] == 0) { --end; }
        oigtl::runtime::ascii::check_bytes(data + off, end - off, "status_message");
        out.status_message.assign(reinterpret_cast<const char*>(data + off), end - off);
        off = length;
    }
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
