// GENERATED from spec/schemas/unit.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/unit.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/ascii.hpp"
#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Unit::pack() const {
    const std::size_t body_size = 8;
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // packed
    oigtl::runtime::byte_order::write_be_u64(out.data() + off, packed);
    off += 8;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Unit Unit::unpack(const std::uint8_t* data, std::size_t length) {
    if (length < kBodySize) {
        std::ostringstream oss;
        oss << "UNIT body requires " << kBodySize
            << " bytes, got " << length;
        throw oigtl::error::ShortBufferError(oss.str());
    }
    Unit out;
    std::size_t off = 0;
    // packed
    if (off + (8) > length) { throw oigtl::error::ShortBufferError("packed: short buffer"); }
    out.packed = oigtl::runtime::byte_order::read_be_u64(data + off);
    off += 8;
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
