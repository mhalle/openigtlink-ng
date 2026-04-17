// GENERATED from spec/schemas/sensor.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/sensor.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Sensor::pack() const {
    const std::size_t body_size = (1) + (1) + (8) + (data.size() * 8);
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // larray
    oigtl::runtime::byte_order::write_be_u8(out.data() + off, larray);
    off += 1;
    // status
    oigtl::runtime::byte_order::write_be_u8(out.data() + off, status);
    off += 1;
    // unit
    oigtl::runtime::byte_order::write_be_u64(out.data() + off, unit);
    off += 8;
    // data
    for (std::size_t i = 0; i < data.size(); ++i) {
        oigtl::runtime::byte_order::write_be_f64(out.data() + off + i * 8, data[i]);
    }
    off += data.size() * 8;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Sensor Sensor::unpack(const std::uint8_t* data, std::size_t length) {
    Sensor out;
    std::size_t off = 0;
    // larray
    if (off + (1) > length) { throw oigtl::error::ShortBufferError("larray: short buffer"); }
    out.larray = oigtl::runtime::byte_order::read_be_u8(data + off);
    off += 1;
    // status
    if (off + (1) > length) { throw oigtl::error::ShortBufferError("status: short buffer"); }
    out.status = oigtl::runtime::byte_order::read_be_u8(data + off);
    off += 1;
    // unit
    if (off + (8) > length) { throw oigtl::error::ShortBufferError("unit: short buffer"); }
    out.unit = oigtl::runtime::byte_order::read_be_u64(data + off);
    off += 8;
    // data
    {
        std::size_t count = static_cast<std::size_t>(out.larray);
        if (off + (count * 8) > length) { throw oigtl::error::ShortBufferError("data: short buffer"); }
        out.data.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            out.data[i] = oigtl::runtime::byte_order::read_be_f64(data + off + i * 8);
        }
        off += count * 8;
    }
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
