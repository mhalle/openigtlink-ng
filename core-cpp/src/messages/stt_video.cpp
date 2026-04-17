// GENERATED from spec/schemas/stt_video.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/stt_video.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/ascii.hpp"
#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> SttVideo::pack() const {
    const std::size_t body_size = 8;
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // codec
    {
        constexpr std::size_t n = 4;
        std::size_t copy_len = codec.size() < n ? codec.size() : n;
        std::memcpy(out.data() + off, codec.data(), copy_len);
        if (copy_len < n) {
            std::memset(out.data() + off + copy_len, 0, n - copy_len);
        }
        off += 4;
    }
    // time_interval
    oigtl::runtime::byte_order::write_be_u32(out.data() + off, time_interval);
    off += 4;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

SttVideo SttVideo::unpack(const std::uint8_t* data, std::size_t length) {
    if (length < kBodySize) {
        std::ostringstream oss;
        oss << "STT_VIDEO body requires " << kBodySize
            << " bytes, got " << length;
        throw oigtl::error::ShortBufferError(oss.str());
    }
    SttVideo out;
    std::size_t off = 0;
    // codec
    if (off + (4) > length) { throw oigtl::error::ShortBufferError("codec: short buffer"); }
    oigtl::runtime::ascii::check_bytes(data + off, 4, "codec");
    out.codec.assign(reinterpret_cast<const char*>(data + off), 4);
    off += 4;
    // time_interval
    if (off + (4) > length) { throw oigtl::error::ShortBufferError("time_interval: short buffer"); }
    out.time_interval = oigtl::runtime::byte_order::read_be_u32(data + off);
    off += 4;
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
