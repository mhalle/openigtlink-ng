// GENERATED from spec/schemas/get_status.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/get_status.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> GetStatus::pack() const {
    const std::size_t body_size = 0;
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

GetStatus GetStatus::unpack(const std::uint8_t* data, std::size_t length) {
    if (length < kBodySize) {
        std::ostringstream oss;
        oss << "GET_STATUS body requires " << kBodySize
            << " bytes, got " << length;
        throw oigtl::error::ShortBufferError(oss.str());
    }
    GetStatus out;
    std::size_t off = 0;
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
