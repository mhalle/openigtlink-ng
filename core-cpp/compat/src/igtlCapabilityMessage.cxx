// Hand-written CAPABILITY facade. Body is N × 12-byte null-padded
// type-id strings.

#include "igtl/igtlCapabilityMessage.h"

#include <algorithm>
#include <cstring>

namespace igtl {

namespace {
constexpr std::size_t kTypeIdSize = 12;
}

CapabilityMessage::CapabilityMessage() {
    m_SendMessageType = "CAPABILITY";
}

void CapabilityMessage::SetTypes(std::vector<std::string> types) {
    m_TypeNames = std::move(types);
}

int CapabilityMessage::SetType(int id, const char* name) {
    if (id < 0 || !name) return 0;
    if (static_cast<std::size_t>(id) >= m_TypeNames.size()) {
        m_TypeNames.resize(static_cast<std::size_t>(id) + 1);
    }
    m_TypeNames[static_cast<std::size_t>(id)] = name;
    return 1;
}

const char* CapabilityMessage::GetType(int id) {
    if (id < 0 ||
        static_cast<std::size_t>(id) >= m_TypeNames.size()) {
        return nullptr;
    }
    return m_TypeNames[static_cast<std::size_t>(id)].c_str();
}

igtlUint64 CapabilityMessage::CalculateContentBufferSize() {
    return m_TypeNames.size() * kTypeIdSize;
}

int CapabilityMessage::PackContent() {
    const std::size_t need = m_TypeNames.size() * kTypeIdSize;
    if (m_Content.size() < need) m_Content.assign(need, 0);
    auto* out = m_Content.data();
    for (std::size_t i = 0; i < m_TypeNames.size(); ++i) {
        std::memset(out + i * kTypeIdSize, 0, kTypeIdSize);
        const auto& s = m_TypeNames[i];
        const std::size_t n = std::min(kTypeIdSize, s.size());
        if (n > 0) std::memcpy(out + i * kTypeIdSize, s.data(), n);
    }
    return 1;
}

int CapabilityMessage::UnpackContent() {
    const std::size_t sz = m_Content.size();
    if (sz % kTypeIdSize != 0) return 0;
    m_TypeNames.clear();
    const auto* in = m_Content.data();
    for (std::size_t off = 0; off < sz; off += kTypeIdSize) {
        std::size_t n = 0;
        while (n < kTypeIdSize && in[off + n] != '\0') ++n;
        m_TypeNames.emplace_back(
            reinterpret_cast<const char*>(in + off), n);
    }
    return 1;
}

}  // namespace igtl
