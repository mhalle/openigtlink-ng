// Hand-written StringMessage facade. Byte-exact with upstream on
// pack; unpack is stricter — we reject OOB length values upstream's
// reference implementation happily OOB-reads past (documented in
// spec/schemas/string.json legacy_notes).

#include "igtl/igtlStringMessage.h"

#include <cstring>

#include "oigtl/runtime/byte_order.hpp"

#ifndef IGTL_STRING_MESSAGE_DEFAULT_ENCODING
#define IGTL_STRING_MESSAGE_DEFAULT_ENCODING 3  // US-ASCII
#endif

namespace igtl {

StringMessage::StringMessage()
    : m_Encoding(IGTL_STRING_MESSAGE_DEFAULT_ENCODING) {
    m_SendMessageType = "STRING";
    m_String.clear();
}

StringMessage::~StringMessage() = default;

// ---- setters / getters ----
int StringMessage::SetString(const char* s) {
    if (!s) { m_String.clear(); return 0; }
    return SetString(std::string(s));
}

int StringMessage::SetString(const std::string& s) {
    if (s.length() > 0xFFFF) return 0;
    m_String = s;
    return static_cast<int>(m_String.length());
}

int StringMessage::SetEncoding(igtlUint16 enc) {
    m_Encoding = enc;
    return 1;
}

const char* StringMessage::GetString()   { return m_String.c_str(); }
igtlUint16  StringMessage::GetEncoding() { return m_Encoding; }

// ---- wire packing ----
igtlUint64 StringMessage::CalculateContentBufferSize() {
    return 4 + m_String.size();
}

int StringMessage::PackContent() {
    namespace bo = oigtl::runtime::byte_order;
    const std::size_t need = 4 + m_String.size();
    if (m_Content.size() < need) m_Content.assign(need, 0);
    auto* out = m_Content.data();

    bo::write_be_u16(out + 0, m_Encoding);
    bo::write_be_u16(out + 2,
        static_cast<igtlUint16>(m_String.size()));
    if (!m_String.empty()) {
        std::memcpy(out + 4, m_String.data(), m_String.size());
    }
    return 1;
}

int StringMessage::UnpackContent() {
    namespace bo = oigtl::runtime::byte_order;
    if (m_Content.size() < 4) return 0;
    const auto* in = m_Content.data();
    m_Encoding = bo::read_be_u16(in + 0);
    const auto declared_len = bo::read_be_u16(in + 2);

    // Stricter-than-upstream: bounds-check length against the
    // available content region. Upstream m_String.append(ptr,
    // declared_len) would OOB-read up to 65535 bytes past the
    // buffer on a too-large declared length.
    const std::size_t available = m_Content.size() - 4;
    if (declared_len > available) return 0;
    m_String.assign(reinterpret_cast<const char*>(in + 4),
                    declared_len);
    return 1;
}

}  // namespace igtl
