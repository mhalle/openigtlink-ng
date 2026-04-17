// Hand-written COMMAND facade.

#include "igtl/igtlCommandMessage.h"

#include <algorithm>
#include <cstring>

#include "oigtl/runtime/byte_order.hpp"

namespace igtl {

namespace {
constexpr std::size_t kFixedSize = 138;
constexpr std::size_t kNameSize  = IGTL_COMMAND_NAME_SIZE;
}

CommandMessage::CommandMessage()
    : m_CommandId(0), m_Encoding(3) {
    m_SendMessageType = "COMMAND";
    std::memset(m_CommandName, 0, sizeof(m_CommandName));
}

int CommandMessage::SetCommandId(igtlUint32 id) {
    m_CommandId = id;
    return 1;
}

int CommandMessage::SetCommandName(const char* n) {
    if (!n) { std::memset(m_CommandName, 0, kNameSize); return 0; }
    std::memset(m_CommandName, 0, kNameSize);
    const std::size_t len =
        std::min(kNameSize, std::strlen(n));
    if (len > 0) std::memcpy(m_CommandName, n, len);
    return 1;
}
int CommandMessage::SetCommandName(const std::string& n) {
    std::memset(m_CommandName, 0, kNameSize);
    const std::size_t len = std::min(kNameSize, n.size());
    if (len > 0) std::memcpy(m_CommandName, n.data(), len);
    return 1;
}
std::string CommandMessage::GetCommandName() const {
    std::size_t len = 0;
    while (len < kNameSize && m_CommandName[len] != '\0') ++len;
    return std::string(
        reinterpret_cast<const char*>(m_CommandName), len);
}

int CommandMessage::SetCommandContent(const char* s) {
    m_Command = s ? s : "";
    return 1;
}
int CommandMessage::SetCommandContent(const std::string& s) {
    m_Command = s;
    return 1;
}

int CommandMessage::SetContentEncoding(igtlUint16 e) {
    m_Encoding = e;
    return 1;
}

igtlUint64 CommandMessage::CalculateContentBufferSize() {
    return kFixedSize + m_Command.size();
}

int CommandMessage::PackContent() {
    namespace bo = oigtl::runtime::byte_order;
    const std::size_t need = kFixedSize + m_Command.size();
    if (m_Content.size() < need) m_Content.assign(need, 0);
    auto* out = m_Content.data();

    bo::write_be_u32(out + 0, m_CommandId);
    std::memcpy(out + 4, m_CommandName, kNameSize);
    bo::write_be_u16(out + 4 + kNameSize, m_Encoding);
    bo::write_be_u32(out + 6 + kNameSize,
                     static_cast<igtlUint32>(m_Command.size()));

    if (!m_Command.empty()) {
        std::memcpy(out + kFixedSize,
                    m_Command.data(), m_Command.size());
    }
    return 1;
}

int CommandMessage::UnpackContent() {
    namespace bo = oigtl::runtime::byte_order;
    if (m_Content.size() < kFixedSize) return 0;
    const auto* in = m_Content.data();
    m_CommandId = bo::read_be_u32(in + 0);
    std::memcpy(m_CommandName, in + 4, kNameSize);
    m_Encoding  = bo::read_be_u16(in + 4 + kNameSize);
    const igtlUint32 len = bo::read_be_u32(in + 6 + kNameSize);
    if (m_Content.size() < kFixedSize + len) return 0;
    m_Command.assign(
        reinterpret_cast<const char*>(in + kFixedSize), len);
    return 1;
}

// ---------------- RTSCommandMessage ----------------
RTSCommandMessage::RTSCommandMessage() {
    m_SendMessageType = "RTS_COMMAND";
}

int RTSCommandMessage::SetCommandErrorString(const char* s) {
    return SetCommandContent(s);
}
int RTSCommandMessage::SetCommandErrorString(const std::string& s) {
    return SetCommandContent(s);
}
std::string RTSCommandMessage::GetCommandErrorString() const {
    return GetCommandContent();
}

}  // namespace igtl
