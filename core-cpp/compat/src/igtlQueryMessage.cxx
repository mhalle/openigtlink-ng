// Hand-written QUERY facade.

#include "igtl/igtlQueryMessage.h"

#include <algorithm>
#include <cstring>

#include "oigtl/runtime/byte_order.hpp"

namespace igtl {

namespace {
constexpr std::size_t kFixedSize = 38;
constexpr std::size_t kDtSize = IGTL_QUERY_DATE_TYPE_SIZE;
}

QueryMessage::QueryMessage() : m_QueryID(0) {
    m_SendMessageType = "QUERY";
    std::memset(m_DataType, 0, kDtSize);
}

int QueryMessage::SetDeviceUID(const char* s) {
    m_DeviceUID = s ? s : "";
    return static_cast<int>(m_DeviceUID.size());
}
int QueryMessage::SetDeviceUID(const std::string& s) {
    m_DeviceUID = s;
    return static_cast<int>(m_DeviceUID.size());
}
std::string QueryMessage::GetDeviceUID() { return m_DeviceUID; }

int QueryMessage::SetDataType(const char* s) {
    if (!s || std::strlen(s) > kDtSize) return 0;
    std::memset(m_DataType, 0, kDtSize);
    std::memcpy(m_DataType, s, std::strlen(s));
    return 1;
}
int QueryMessage::SetDataType(const std::string& s) {
    if (s.size() > kDtSize) return 0;
    std::memset(m_DataType, 0, kDtSize);
    if (!s.empty()) std::memcpy(m_DataType, s.data(), s.size());
    return 1;
}
std::string QueryMessage::GetDataType() {
    std::size_t n = 0;
    while (n < kDtSize && m_DataType[n] != '\0') ++n;
    return std::string(
        reinterpret_cast<const char*>(m_DataType), n);
}

igtlUint64 QueryMessage::CalculateContentBufferSize() {
    return kFixedSize + m_DeviceUID.size();
}

int QueryMessage::PackContent() {
    namespace bo = oigtl::runtime::byte_order;
    const std::size_t need = kFixedSize + m_DeviceUID.size();
    if (m_Content.size() < need) m_Content.assign(need, 0);
    auto* out = m_Content.data();

    bo::write_be_u32(out + 0, m_QueryID);
    std::memcpy(out + 4, m_DataType, kDtSize);
    bo::write_be_u16(out + 4 + kDtSize,
                     static_cast<igtlUint16>(m_DeviceUID.size()));
    if (!m_DeviceUID.empty()) {
        std::memcpy(out + kFixedSize,
                    m_DeviceUID.data(), m_DeviceUID.size());
    }
    return 1;
}

int QueryMessage::UnpackContent() {
    namespace bo = oigtl::runtime::byte_order;
    if (m_Content.size() < kFixedSize) return 0;
    const auto* in = m_Content.data();
    m_QueryID = bo::read_be_u32(in + 0);
    std::memcpy(m_DataType, in + 4, kDtSize);
    const igtlUint16 uid_len = bo::read_be_u16(in + 4 + kDtSize);
    if (m_Content.size() < kFixedSize + uid_len) return 0;
    m_DeviceUID.assign(
        reinterpret_cast<const char*>(in + kFixedSize), uid_len);
    return 1;
}

}  // namespace igtl
