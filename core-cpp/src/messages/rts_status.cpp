// GENERATED from spec/schemas/rts_status.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/rts_status.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> RtsStatus::pack() const {
    const std::size_t body_size = 1;
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // status
    oigtl::runtime::byte_order::write_be_i8(out.data() + off, status);
    off += 1;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

RtsStatus RtsStatus::unpack(const std::uint8_t* data, std::size_t length) {
    if (length < kBodySize) {
        std::ostringstream oss;
        oss << "RTS_STATUS body requires " << kBodySize
            << " bytes, got " << length;
        throw oigtl::error::ShortBufferError(oss.str());
    }
    RtsStatus out;
    std::size_t off = 0;
    // status
    if (off + (1) > length) { throw oigtl::error::ShortBufferError("status: short buffer"); }
    out.status = oigtl::runtime::byte_order::read_be_i8(data + off);
    off += 1;
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
