// Hand-written StatusMessage facade. Pack/Unpack match upstream
// byte-for-byte; SetErrorName mirrors upstream's strncpy behaviour
// (20-byte destination, no guaranteed null-termination if the
// input is ≥ 20 chars — documented in the schema's legacy_notes).

#include "igtl/igtlStatusMessage.h"

#include <cstring>

#include "oigtl/runtime/byte_order.hpp"

namespace igtl {

StatusMessage::StatusMessage()
    : m_Code(0), m_SubCode(0) {
    m_SendMessageType = "STATUS";
    std::memset(m_ErrorName, 0, sizeof(m_ErrorName));
}

StatusMessage::~StatusMessage() = default;

// ---- setters / getters ----
void StatusMessage::SetCode(int code) {
    m_Code = static_cast<igtlUint16>(code);
}
int StatusMessage::GetCode() { return m_Code; }

void StatusMessage::SetSubCode(igtlInt64 sc) { m_SubCode = sc; }
igtlInt64 StatusMessage::GetSubCode() { return m_SubCode; }

void StatusMessage::SetErrorName(const char* name) {
    // Upstream behaviour (matched exactly for compat):
    //   m_ErrorName[19] = '\0';
    //   strncpy(m_ErrorName, name, 20);
    // The pre-set of index 19 is overwritten by strncpy if
    // `name` has ≥ 20 chars. If `name` has fewer, strncpy
    // null-pads the tail.
    if (!name) { m_ErrorName[0] = '\0'; return; }
    m_ErrorName[19] = '\0';
    std::strncpy(m_ErrorName, name, 20);
}
const char* StatusMessage::GetErrorName() {
    // Defensive null-termination for callers who c_str() this.
    m_ErrorName[19] = '\0';
    return m_ErrorName;
}

void StatusMessage::SetStatusString(const char* s) {
    m_StatusMessageString = s ? s : "";
}
const char* StatusMessage::GetStatusString() {
    return m_StatusMessageString.c_str();
}

// ---- wire packing ----
// Fixed header is exactly 30 bytes (2+8+20), then a null-terminated
// trailing string. Body size includes the terminator.
static constexpr std::size_t kFixedSize = 30;

igtlUint64 StatusMessage::CalculateContentBufferSize() {
    return kFixedSize + m_StatusMessageString.size() + 1;
}

int StatusMessage::PackContent() {
    namespace bo = oigtl::runtime::byte_order;
    const std::size_t need = kFixedSize +
                             m_StatusMessageString.size() + 1;
    if (m_Content.size() < need) m_Content.assign(need, 0);
    auto* out = m_Content.data();

    bo::write_be_u16(out + 0, m_Code);
    bo::write_be_i64(out + 2, m_SubCode);
    // 20-byte error_name field. Upstream writes via strncpy
    // (null-pads short inputs, doesn't null-terminate full ones).
    // We replicate that via memcpy over a zero-initialised region.
    std::memset(out + 10, 0, 20);
    // Match upstream's strncpy(dst, src, 20) semantics: copy up to
    // the first NUL or 20 bytes, whichever comes first.
    std::size_t n = 0;
    while (n < 20 && m_ErrorName[n] != '\0') ++n;
    if (n > 0) std::memcpy(out + 10, m_ErrorName, n);

    // Trailing string + explicit NUL terminator.
    if (!m_StatusMessageString.empty()) {
        std::memcpy(out + kFixedSize,
                    m_StatusMessageString.data(),
                    m_StatusMessageString.size());
    }
    out[kFixedSize + m_StatusMessageString.size()] = '\0';
    return 1;
}

int StatusMessage::UnpackContent() {
    namespace bo = oigtl::runtime::byte_order;
    if (m_Content.size() < kFixedSize) return 0;
    const auto* in = m_Content.data();
    m_Code    = bo::read_be_u16(in + 0);
    m_SubCode = bo::read_be_i64(in + 2);

    std::memcpy(m_ErrorName, in + 10, 20);
    m_ErrorName[19] = '\0';  // defensive

    // Trailing string: everything after byte 30, up to (but not
    // including) the final NUL. Upstream treats a missing
    // terminator as a silent-discard; we match that behaviour.
    const std::size_t remaining = m_Content.size() - kFixedSize;
    if (remaining == 0) {
        m_StatusMessageString.clear();
    } else {
        // Strip the final NUL if present.
        std::size_t len = remaining;
        if (in[m_Content.size() - 1] == '\0') --len;
        m_StatusMessageString.assign(
            reinterpret_cast<const char*>(in + kFixedSize), len);
    }
    return 1;
}

}  // namespace igtl
