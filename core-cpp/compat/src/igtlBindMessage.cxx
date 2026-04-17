// Hand-written BIND facade.

#include "igtl/igtlBindMessage.h"

#include <algorithm>
#include <cstring>

#include "oigtl/runtime/byte_order.hpp"

namespace igtl {

namespace bo = oigtl::runtime::byte_order;

namespace {
constexpr std::size_t kHeaderEntrySize = 20;  // 12 type_id + 8 body_size

std::size_t round_up_even(std::size_t n) {
    return (n + 1) & ~std::size_t(1);
}

void write_fixed_type(std::uint8_t* out, const std::string& t) {
    std::memset(out, 0, 12);
    const std::size_t n = std::min<std::size_t>(12, t.size());
    if (n > 0) std::memcpy(out, t.data(), n);
}

std::string read_fixed_type(const std::uint8_t* in) {
    std::size_t n = 0;
    while (n < 12 && in[n] != '\0') ++n;
    return std::string(reinterpret_cast<const char*>(in), n);
}
}  // namespace

// ============ BindMessageBase ============
BindMessageBase::BindMessageBase() {
    m_SendMessageType = "BIND";
}

void BindMessageBase::Init() { m_ChildMessages.clear(); }

int BindMessageBase::SetNumberOfChildMessages(unsigned int n) {
    m_ChildMessages.resize(n);
    return static_cast<int>(m_ChildMessages.size());
}
int BindMessageBase::GetNumberOfChildMessages() {
    return static_cast<int>(m_ChildMessages.size());
}

int BindMessageBase::AppendChildMessage(MessageBase* child) {
    if (!child) return 0;
    ChildMessageInfo info;
    info.type = child->GetMessageType();
    info.name = child->GetDeviceName();
    // Make sure child is packed; then copy body bytes.
    child->Pack();
    const auto body_size = child->GetPackBodySize();
    const auto* body_ptr = static_cast<const std::uint8_t*>(
        child->GetPackBodyPointer());
    if (body_ptr && body_size > 0) {
        info.body.assign(body_ptr, body_ptr + body_size);
    }
    m_ChildMessages.push_back(std::move(info));
    return static_cast<int>(m_ChildMessages.size());
}

int BindMessageBase::SetChildMessage(unsigned int i, MessageBase* child) {
    if (!child) return 0;
    if (i >= m_ChildMessages.size()) {
        m_ChildMessages.resize(i + 1);
    }
    ChildMessageInfo info;
    info.type = child->GetMessageType();
    info.name = child->GetDeviceName();
    child->Pack();
    const auto body_size = child->GetPackBodySize();
    const auto* body_ptr = static_cast<const std::uint8_t*>(
        child->GetPackBodyPointer());
    if (body_ptr && body_size > 0) {
        info.body.assign(body_ptr, body_ptr + body_size);
    }
    m_ChildMessages[i] = std::move(info);
    return 1;
}

const char* BindMessageBase::GetChildMessageType(unsigned int i) {
    if (i >= m_ChildMessages.size()) return nullptr;
    return m_ChildMessages[i].type.c_str();
}

int BindMessageBase::pack_impl(bool with_bodies) {
    const auto ncmsg = m_ChildMessages.size();
    std::size_t name_table_bytes = 0;
    for (auto& c : m_ChildMessages) {
        name_table_bytes += c.name.size() + 1;
    }
    name_table_bytes = round_up_even(name_table_bytes);

    std::size_t bodies_bytes = 0;
    if (with_bodies) {
        for (auto& c : m_ChildMessages) {
            bodies_bytes += round_up_even(c.body.size());
        }
    }
    const std::size_t total =
        2 + ncmsg * kHeaderEntrySize + 2 +
        name_table_bytes + bodies_bytes;
    if (m_Content.size() < total) m_Content.assign(total, 0);
    auto* out = m_Content.data();

    bo::write_be_u16(out, static_cast<std::uint16_t>(ncmsg));
    auto* entries = out + 2;
    for (std::size_t i = 0; i < ncmsg; ++i) {
        auto& c = m_ChildMessages[i];
        write_fixed_type(entries + i * kHeaderEntrySize, c.type);
        bo::write_be_u64(entries + i * kHeaderEntrySize + 12,
                         with_bodies ? c.body.size() : 0);
    }
    std::uint8_t* cur = entries + ncmsg * kHeaderEntrySize;
    bo::write_be_u16(cur,
        static_cast<std::uint16_t>(name_table_bytes));
    cur += 2;

    // Name table — null-terminated names, padded to even.
    std::uint8_t* name_cur = cur;
    for (auto& c : m_ChildMessages) {
        if (!c.name.empty()) {
            std::memcpy(name_cur, c.name.data(), c.name.size());
        }
        name_cur += c.name.size();
        *name_cur++ = '\0';
    }
    // Any padding bytes in name_table are already zero (buffer was
    // zero-initialised via assign).
    cur += name_table_bytes;

    if (with_bodies) {
        for (auto& c : m_ChildMessages) {
            if (!c.body.empty()) {
                std::memcpy(cur, c.body.data(), c.body.size());
            }
            cur += round_up_even(c.body.size());
        }
    }
    return 1;
}

int BindMessageBase::unpack_impl(bool with_bodies) {
    if (m_Content.size() < 2) return 0;
    const auto* in = m_Content.data();
    const std::size_t ncmsg = bo::read_be_u16(in);
    if (m_Content.size() < 2 + ncmsg * kHeaderEntrySize + 2) return 0;
    m_ChildMessages.clear();
    m_ChildMessages.resize(ncmsg);
    const auto* entries = in + 2;
    std::vector<std::size_t> body_sizes(ncmsg);
    for (std::size_t i = 0; i < ncmsg; ++i) {
        m_ChildMessages[i].type =
            read_fixed_type(entries + i * kHeaderEntrySize);
        body_sizes[i] = static_cast<std::size_t>(
            bo::read_be_u64(entries + i * kHeaderEntrySize + 12));
    }
    const auto* cur = entries + ncmsg * kHeaderEntrySize;
    const std::size_t nt_size = bo::read_be_u16(cur);
    cur += 2;
    if (cur + nt_size > in + m_Content.size()) return 0;

    // Split name table into ncmsg names.
    const auto* nt = cur;
    for (std::size_t i = 0; i < ncmsg; ++i) {
        std::size_t n = 0;
        while (static_cast<std::size_t>((nt + n) - cur) < nt_size &&
               nt[n] != '\0') {
            ++n;
        }
        m_ChildMessages[i].name =
            std::string(reinterpret_cast<const char*>(nt), n);
        nt += n + 1;
    }
    cur += nt_size;

    if (with_bodies) {
        for (std::size_t i = 0; i < ncmsg; ++i) {
            const std::size_t sz = body_sizes[i];
            if (cur + sz > in + m_Content.size()) return 0;
            m_ChildMessages[i].body.assign(cur, cur + sz);
            cur += round_up_even(sz);
        }
    }
    return 1;
}

igtlUint64 BindMessageBase::CalculateContentBufferSize() {
    // Conservative: compute as if packing with bodies.
    std::size_t total = 2 + m_ChildMessages.size() * kHeaderEntrySize + 2;
    std::size_t nt = 0;
    for (auto& c : m_ChildMessages) nt += c.name.size() + 1;
    total += round_up_even(nt);
    for (auto& c : m_ChildMessages) total += round_up_even(c.body.size());
    return total;
}

int BindMessageBase::PackContent()   { return pack_impl(true); }
int BindMessageBase::UnpackContent() { return unpack_impl(true); }

// ============ BindMessage ============
BindMessage::BindMessage() {
    m_SendMessageType = "BIND";
}

int BindMessage::GetChildMessage(unsigned int i, MessageBase* child) {
    if (i >= m_ChildMessages.size() || !child) return 0;
    // Caller is expected to have prepared `child` with the correct
    // type; we populate its state by round-tripping through Unpack
    // if possible. The simplest correct thing is to copy our body
    // bytes into the caller's content region and call UnpackContent.
    // MessageBase doesn't expose a direct setter for m_Content, so
    // we require the caller to pre-set device name from
    // GetChildMessageType and use SetMessageHeader / Unpack flow.
    // For now, return the body via the side accessor.
    (void)child;
    return 0;
}

const std::vector<std::uint8_t>*
BindMessage::GetChildBody(unsigned int i) const {
    if (i >= m_ChildMessages.size()) return nullptr;
    return &m_ChildMessages[i].body;
}

// ============ GetBindMessage ============
GetBindMessage::GetBindMessage() {
    m_SendMessageType = "GET_BIND";
}

igtlUint64 GetBindMessage::CalculateContentBufferSize() {
    // Header entries + name table only (no bodies).
    std::size_t total = 2 + m_ChildMessages.size() * kHeaderEntrySize + 2;
    std::size_t nt = 0;
    for (auto& c : m_ChildMessages) nt += c.name.size() + 1;
    total += round_up_even(nt);
    return total;
}

int GetBindMessage::PackContent()   { return pack_impl(false); }
int GetBindMessage::UnpackContent() { return unpack_impl(false); }

// ============ StartBindMessage ============
StartBindMessage::StartBindMessage() : m_Resolution(0) {
    m_SendMessageType = "STT_BIND";
}

igtlUint64 StartBindMessage::CalculateContentBufferSize() {
    // Parent's content + 8-byte trailing resolution.
    return GetBindMessage::CalculateContentBufferSize() + 8;
}

int StartBindMessage::PackContent() {
    // First pack GET_BIND content into m_Content, then append
    // the 8-byte resolution.
    const auto base_size =
        GetBindMessage::CalculateContentBufferSize();
    m_Content.assign(static_cast<std::size_t>(base_size + 8), 0);
    pack_impl(false);
    bo::write_be_u64(m_Content.data() + base_size, m_Resolution);
    return 1;
}

int StartBindMessage::UnpackContent() {
    if (m_Content.size() < 8) return 0;
    const auto trailer_off = m_Content.size() - 8;
    m_Resolution = bo::read_be_u64(m_Content.data() + trailer_off);
    // Temporarily truncate m_Content for parent's unpack, then
    // restore.
    auto save = m_Content;
    m_Content.resize(trailer_off);
    const int r = unpack_impl(false);
    m_Content = std::move(save);
    return r;
}

// ============ RTSBindMessage ============
RTSBindMessage::RTSBindMessage() : m_Status(0) {
    m_SendMessageType = "RTS_BIND";
}
int RTSBindMessage::PackContent() {
    if (m_Content.size() < 1) m_Content.assign(1, 0);
    m_Content[0] = m_Status;
    return 1;
}
int RTSBindMessage::UnpackContent() {
    if (m_Content.size() < 1) return 0;
    m_Status = m_Content[0];
    return 1;
}

}  // namespace igtl
