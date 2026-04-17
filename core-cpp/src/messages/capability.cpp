// GENERATED from spec/schemas/capability.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/capability.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/ascii.hpp"
#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Capability::pack() const {
    const std::size_t body_size = (supported_types.size() * 12);
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // supported_types
    for (std::size_t i = 0; i < supported_types.size(); ++i) {
        {
            constexpr std::size_t n = 12;
            std::size_t copy_len = supported_types[i].size() < n ? supported_types[i].size() : n;
            std::memcpy(out.data() + off, supported_types[i].data(), copy_len);
            if (copy_len < n) {
                std::memset(out.data() + off + copy_len, 0, n - copy_len);
            }
            off += 12;
        }
    }
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Capability Capability::unpack(const std::uint8_t* data, std::size_t length) {
    Capability out;
    std::size_t off = 0;
    // supported_types
    {
        std::size_t count = (length - off) / 12;
        if (off + (count * 12) > length) { throw oigtl::error::ShortBufferError("supported_types: short buffer"); }
        out.supported_types.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            {
            constexpr std::size_t n = 12;
            std::size_t len = 0;
            while (len < n && data[off + len] != 0) { ++len; }
            out.supported_types[i].assign(reinterpret_cast<const char*>(data + off), len);
            off += 12;
            }
        }
    }
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
