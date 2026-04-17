// Hand-written LBMETA facade. Body: N × 116-byte elements.
//   64 B  name
//   20 B  device_name
//    1 B  label
//    1 B  reserved
//    4 B  rgba
//    6 B  size[3] (u16)
//   20 B  owner

#include "igtl/igtlLabelMetaMessage.h"

#include <algorithm>
#include <cstring>

#include "oigtl/runtime/byte_order.hpp"

namespace igtl {

namespace {
constexpr std::size_t kElementSize = 116;
constexpr std::size_t kLenName     = 64;
constexpr std::size_t kLenDevice   = 20;
constexpr std::size_t kLenOwner    = 20;

void fs_pack(std::uint8_t* dst, std::size_t n,
             const std::string& src) {
    std::memset(dst, 0, n);
    const std::size_t len = std::min(n, src.size());
    if (len > 0) std::memcpy(dst, src.data(), len);
}
std::string fs_unpack(const std::uint8_t* src, std::size_t n) {
    std::size_t len = 0;
    while (len < n && src[len] != '\0') ++len;
    return std::string(reinterpret_cast<const char*>(src), len);
}
}  // namespace

// ---------------- LabelMetaElement ----------------
LabelMetaElement::LabelMetaElement()
    : m_Label(0) {
    m_RGBA[0] = m_RGBA[1] = m_RGBA[2] = m_RGBA[3] = 0;
    m_Size[0] = m_Size[1] = m_Size[2] = 0;
}

int LabelMetaElement::SetName(const char* n) {
    m_Name = n ? n : "";
    return static_cast<int>(m_Name.size());
}
int LabelMetaElement::SetDeviceName(const char* d) {
    m_DeviceName = d ? d : "";
    return static_cast<int>(m_DeviceName.size());
}
int LabelMetaElement::SetOwner(const char* o) {
    m_Owner = o ? o : "";
    return static_cast<int>(m_Owner.size());
}

void LabelMetaElement::SetRGBA(igtlUint8 rgba[4]) {
    m_RGBA[0] = rgba[0]; m_RGBA[1] = rgba[1];
    m_RGBA[2] = rgba[2]; m_RGBA[3] = rgba[3];
}
void LabelMetaElement::SetRGBA(igtlUint8 r, igtlUint8 g,
                                igtlUint8 b, igtlUint8 a) {
    m_RGBA[0] = r; m_RGBA[1] = g; m_RGBA[2] = b; m_RGBA[3] = a;
}
void LabelMetaElement::GetRGBA(igtlUint8* rgba) {
    rgba[0] = m_RGBA[0]; rgba[1] = m_RGBA[1];
    rgba[2] = m_RGBA[2]; rgba[3] = m_RGBA[3];
}
void LabelMetaElement::GetRGBA(igtlUint8& r, igtlUint8& g,
                                igtlUint8& b, igtlUint8& a) {
    r = m_RGBA[0]; g = m_RGBA[1]; b = m_RGBA[2]; a = m_RGBA[3];
}

void LabelMetaElement::SetSize(igtlUint16 s[3]) {
    m_Size[0] = s[0]; m_Size[1] = s[1]; m_Size[2] = s[2];
}
void LabelMetaElement::SetSize(igtlUint16 si, igtlUint16 sj,
                                igtlUint16 sk) {
    m_Size[0] = si; m_Size[1] = sj; m_Size[2] = sk;
}
void LabelMetaElement::GetSize(igtlUint16* s) {
    s[0] = m_Size[0]; s[1] = m_Size[1]; s[2] = m_Size[2];
}

// ---------------- LabelMetaMessage ----------------
LabelMetaMessage::LabelMetaMessage() {
    m_SendMessageType = "LBMETA";
}

int LabelMetaMessage::AddLabelMetaElement(LabelMetaElement::Pointer& e) {
    m_LabelMetaList.push_back(e);
    return static_cast<int>(m_LabelMetaList.size());
}
void LabelMetaMessage::ClearLabelMetaElement() {
    m_LabelMetaList.clear();
}
int LabelMetaMessage::GetNumberOfLabelMetaElement() {
    return static_cast<int>(m_LabelMetaList.size());
}
void LabelMetaMessage::GetLabelMetaElement(
        int i, LabelMetaElement::Pointer& e) {
    if (i < 0 || static_cast<std::size_t>(i) >= m_LabelMetaList.size()) {
        e = LabelMetaElement::Pointer();
        return;
    }
    e = m_LabelMetaList[i];
}

igtlUint64 LabelMetaMessage::CalculateContentBufferSize() {
    return m_LabelMetaList.size() * kElementSize;
}

int LabelMetaMessage::PackContent() {
    namespace bo = oigtl::runtime::byte_order;
    const std::size_t need = m_LabelMetaList.size() * kElementSize;
    if (m_Content.size() < need) m_Content.assign(need, 0);
    auto* out = m_Content.data();

    for (auto& e : m_LabelMetaList) {
        std::size_t off = 0;
        fs_pack(out + off, kLenName,   e->GetName());       off += kLenName;
        fs_pack(out + off, kLenDevice, e->GetDeviceName()); off += kLenDevice;
        out[off++] = e->GetLabel();
        out[off++] = 0;  // reserved
        igtlUint8 rgba[4]; e->GetRGBA(rgba);
        out[off++] = rgba[0]; out[off++] = rgba[1];
        out[off++] = rgba[2]; out[off++] = rgba[3];
        igtlUint16 sz[3]; e->GetSize(sz);
        bo::write_be_u16(out + off + 0, sz[0]);
        bo::write_be_u16(out + off + 2, sz[1]);
        bo::write_be_u16(out + off + 4, sz[2]);
        off += 6;
        fs_pack(out + off, kLenOwner, e->GetOwner()); off += kLenOwner;
        out += kElementSize;
        (void)off;
    }
    return 1;
}

int LabelMetaMessage::UnpackContent() {
    namespace bo = oigtl::runtime::byte_order;
    const std::size_t sz = m_Content.size();
    if (sz % kElementSize != 0) return 0;
    ClearLabelMetaElement();
    const std::size_t n = sz / kElementSize;
    const auto* in = m_Content.data();
    for (std::size_t i = 0; i < n; ++i) {
        auto e = LabelMetaElement::New();
        std::size_t off = 0;
        e->SetName(fs_unpack(in + off, kLenName).c_str());         off += kLenName;
        e->SetDeviceName(fs_unpack(in + off, kLenDevice).c_str()); off += kLenDevice;
        e->SetLabel(in[off]); off += 2;  // +reserved
        e->SetRGBA(in[off], in[off+1], in[off+2], in[off+3]); off += 4;
        igtlUint16 s[3];
        s[0] = bo::read_be_u16(in + off + 0);
        s[1] = bo::read_be_u16(in + off + 2);
        s[2] = bo::read_be_u16(in + off + 4);
        off += 6;
        e->SetSize(s);
        e->SetOwner(fs_unpack(in + off, kLenOwner).c_str()); off += kLenOwner;
        (void)off;
        m_LabelMetaList.push_back(e);
        in += kElementSize;
    }
    return 1;
}

}  // namespace igtl
