// GENERATED from spec/schemas/stt_qtdata.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/stt_qtdata.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> SttQtdata::pack() const {
    const std::size_t body_size = 36;
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // resolution
    oigtl::runtime::byte_order::write_be_i32(out.data() + off, resolution);
    off += 4;
    // coord_name
    {
        constexpr std::size_t n = 32;
        std::size_t copy_len = coord_name.size() < n ? coord_name.size() : n;
        std::memcpy(out.data() + off, coord_name.data(), copy_len);
        if (copy_len < n) {
            std::memset(out.data() + off + copy_len, 0, n - copy_len);
        }
        off += 32;
    }
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

SttQtdata SttQtdata::unpack(const std::uint8_t* data, std::size_t length) {
    if (length < kBodySize) {
        std::ostringstream oss;
        oss << "STT_QTDATA body requires " << kBodySize
            << " bytes, got " << length;
        throw oigtl::error::ShortBufferError(oss.str());
    }
    SttQtdata out;
    std::size_t off = 0;
    // resolution
    if (off + (4) > length) { throw oigtl::error::ShortBufferError("resolution: short buffer"); }
    out.resolution = oigtl::runtime::byte_order::read_be_i32(data + off);
    off += 4;
    // coord_name
    if (off + (32) > length) { throw oigtl::error::ShortBufferError("coord_name: short buffer"); }
    {
        constexpr std::size_t n = 32;
        std::size_t len = 0;
        while (len < n && data[off + len] != 0) { ++len; }
        out.coord_name.assign(reinterpret_cast<const char*>(data + off), len);
        off += 32;
    }
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
