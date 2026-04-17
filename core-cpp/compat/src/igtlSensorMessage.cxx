// Hand-written SENSOR facade. Body: 10-byte header (larray, status,
// unit) + larray × float64.

#include "igtl/igtlSensorMessage.h"

#include <cstring>

#include "oigtl/runtime/byte_order.hpp"

namespace igtl {

SensorMessage::SensorMessage()
    : m_Unit(0), m_Status(0) {
    m_SendMessageType = "SENSOR";
}

int SensorMessage::SetLength(unsigned int n) {
    m_Array.resize(n, 0.0);
    return static_cast<int>(m_Array.size());
}

unsigned int SensorMessage::GetLength() {
    return static_cast<unsigned int>(m_Array.size());
}

int SensorMessage::SetUnit(igtlUnit u) { m_Unit = u; return 1; }

int SensorMessage::SetValue(const igtlFloat64* data) {
    if (!data) return 0;
    for (std::size_t i = 0; i < m_Array.size(); ++i) m_Array[i] = data[i];
    return 1;
}
int SensorMessage::SetValue(unsigned int i, igtlFloat64 v) {
    if (i >= m_Array.size()) return 0;
    m_Array[i] = v;
    return 1;
}
igtlFloat64 SensorMessage::GetValue(unsigned int i) {
    if (i >= m_Array.size()) return 0.0;
    return m_Array[i];
}

igtlUint64 SensorMessage::CalculateContentBufferSize() {
    return 10 + m_Array.size() * 8;
}

int SensorMessage::PackContent() {
    namespace bo = oigtl::runtime::byte_order;
    const std::size_t need = 10 + m_Array.size() * 8;
    if (m_Content.size() < need) m_Content.assign(need, 0);
    auto* out = m_Content.data();
    out[0] = static_cast<std::uint8_t>(m_Array.size() & 0xff);
    out[1] = m_Status;
    bo::write_be_u64(out + 2, m_Unit);
    for (std::size_t i = 0; i < m_Array.size(); ++i) {
        bo::write_be_f64(out + 10 + i * 8, m_Array[i]);
    }
    return 1;
}

int SensorMessage::UnpackContent() {
    namespace bo = oigtl::runtime::byte_order;
    if (m_Content.size() < 10) return 0;
    const auto* in = m_Content.data();
    const std::size_t larray = in[0];
    m_Status = in[1];
    m_Unit   = bo::read_be_u64(in + 2);
    // Validate length.
    if (m_Content.size() < 10 + larray * 8) return 0;
    m_Array.resize(larray);
    for (std::size_t i = 0; i < larray; ++i) {
        m_Array[i] = bo::read_be_f64(in + 10 + i * 8);
    }
    return 1;
}

}  // namespace igtl
