// GENERATED from spec/schemas/get_qtrans.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/get_qtrans.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/ascii.hpp"
#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> GetQtrans::pack() const {
    const std::size_t body_size = 0;
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

GetQtrans GetQtrans::unpack(const std::uint8_t* data, std::size_t length) {
    if (length < kBodySize) {
        std::ostringstream oss;
        oss << "GET_QTRANS body requires " << kBodySize
            << " bytes, got " << length;
        throw oigtl::error::ShortBufferError(oss.str());
    }
    GetQtrans out;
    std::size_t off = 0;
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
