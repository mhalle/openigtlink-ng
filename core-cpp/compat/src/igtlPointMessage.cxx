// Hand-written POINT facade. Body: N × 136-byte elements.
// Layout per element (spec/schemas/point.json):
//   64 B  name        (fixed_string, null-padded)
//   32 B  group_name  (fixed_string, null-padded)
//    4 B  rgba        (uint8[4])
//   12 B  position    (float32[3], big-endian)
//    4 B  radius      (float32, big-endian)
//   20 B  owner       (fixed_string, null-padded)
// --------
//  136 B total.
//
// Upstream uses strncpy on the three string fields; short inputs
// get zero-padded to the fixed size, exactly-sized inputs are NOT
// null-terminated. We match that behaviour byte-for-byte.

#include "igtl/igtlPointMessage.h"

#include <cstring>

#include "oigtl/runtime/byte_order.hpp"

namespace igtl {

namespace {
constexpr std::size_t kElementSize = 136;
constexpr std::size_t kLenName     = 64;
constexpr std::size_t kLenGroup    = 32;
constexpr std::size_t kLenOwner    = 20;

// strncpy(dst, src, N) semantics over a zero-initialised region.
// - If strlen(src) < N:   first strlen(src) bytes copied, rest
//                         zero (as pre-initialised).
// - If strlen(src) >= N:  first N bytes copied, no terminator.
void fixed_string_pack(std::uint8_t* dst, std::size_t n,
                       const std::string& src) {
    std::memset(dst, 0, n);
    const std::size_t copy_len = std::min(n, src.size());
    if (copy_len > 0) std::memcpy(dst, src.data(), copy_len);
}

std::string fixed_string_unpack(const std::uint8_t* src,
                                std::size_t n) {
    // Treat as null-padded: truncate at first NUL, else take all n.
    std::size_t len = 0;
    while (len < n && src[len] != '\0') ++len;
    return std::string(reinterpret_cast<const char*>(src), len);
}
}  // namespace

// ---------------- PointElement ----------------
PointElement::PointElement()
    : m_Radius(0.0f) {
    m_RGBA[0] = m_RGBA[1] = m_RGBA[2] = 0;
    m_RGBA[3] = 255;   // opaque default, matches upstream's ctor
    m_Position[0] = m_Position[1] = m_Position[2] = 0.0f;
}

int PointElement::SetName(const char* n) {
    m_Name = n ? n : "";  return static_cast<int>(m_Name.length());
}
int PointElement::SetGroupName(const char* g) {
    m_GroupName = g ? g : "";
    return static_cast<int>(m_GroupName.length());
}
int PointElement::SetOwner(const char* o) {
    m_Owner = o ? o : "";  return static_cast<int>(m_Owner.length());
}

void PointElement::SetRGBA(igtlUint8 rgba[4]) {
    m_RGBA[0] = rgba[0]; m_RGBA[1] = rgba[1];
    m_RGBA[2] = rgba[2]; m_RGBA[3] = rgba[3];
}
void PointElement::SetRGBA(igtlUint8 r, igtlUint8 g,
                           igtlUint8 b, igtlUint8 a) {
    m_RGBA[0] = r; m_RGBA[1] = g; m_RGBA[2] = b; m_RGBA[3] = a;
}
void PointElement::GetRGBA(igtlUint8* rgba) {
    rgba[0] = m_RGBA[0]; rgba[1] = m_RGBA[1];
    rgba[2] = m_RGBA[2]; rgba[3] = m_RGBA[3];
}
void PointElement::GetRGBA(igtlUint8& r, igtlUint8& g,
                           igtlUint8& b, igtlUint8& a) {
    r = m_RGBA[0]; g = m_RGBA[1]; b = m_RGBA[2]; a = m_RGBA[3];
}

void PointElement::SetPosition(igtlFloat32 p[3]) {
    m_Position[0] = p[0]; m_Position[1] = p[1]; m_Position[2] = p[2];
}
void PointElement::SetPosition(igtlFloat32 x, igtlFloat32 y,
                               igtlFloat32 z) {
    m_Position[0] = x; m_Position[1] = y; m_Position[2] = z;
}
void PointElement::GetPosition(igtlFloat32* p) {
    p[0] = m_Position[0]; p[1] = m_Position[1]; p[2] = m_Position[2];
}
void PointElement::GetPosition(igtlFloat32& x, igtlFloat32& y,
                               igtlFloat32& z) {
    x = m_Position[0]; y = m_Position[1]; z = m_Position[2];
}

// ---------------- PointMessage ----------------
PointMessage::PointMessage() {
    m_SendMessageType = "POINT";
}

int PointMessage::AddPointElement(PointElement::Pointer& elem) {
    m_PointList.push_back(elem);
    return static_cast<int>(m_PointList.size());
}

void PointMessage::ClearPointElements() { m_PointList.clear(); }

int PointMessage::GetNumberOfPointElement() {
    return static_cast<int>(m_PointList.size());
}

void PointMessage::GetPointElement(int index,
                                   PointElement::Pointer& elem) {
    if (index < 0 || static_cast<std::size_t>(index) >=
                     m_PointList.size()) {
        elem = PointElement::Pointer();
        return;
    }
    elem = m_PointList[index];
}

igtlUint64 PointMessage::CalculateContentBufferSize() {
    return m_PointList.size() * kElementSize;
}

int PointMessage::PackContent() {
    namespace bo = oigtl::runtime::byte_order;
    const std::size_t need = m_PointList.size() * kElementSize;
    if (m_Content.size() < need) m_Content.assign(need, 0);
    auto* out = m_Content.data();

    for (auto& p : m_PointList) {
        fixed_string_pack(out + 0,  kLenName,  p->GetName());
        fixed_string_pack(out + 64, kLenGroup, p->GetGroupName());

        igtlUint8 rgba[4]; p->GetRGBA(rgba);
        out[96]  = rgba[0]; out[97] = rgba[1];
        out[98]  = rgba[2]; out[99] = rgba[3];

        igtlFloat32 pos[3]; p->GetPosition(pos);
        bo::write_be_f32(out + 100, pos[0]);
        bo::write_be_f32(out + 104, pos[1]);
        bo::write_be_f32(out + 108, pos[2]);

        bo::write_be_f32(out + 112, p->GetRadius());

        fixed_string_pack(out + 116, kLenOwner, p->GetOwner());

        out += kElementSize;
    }
    return 1;
}

int PointMessage::UnpackContent() {
    namespace bo = oigtl::runtime::byte_order;
    const std::size_t sz = m_Content.size();
    if (sz % kElementSize != 0) return 0;   // malformed

    ClearPointElements();
    const std::size_t n = sz / kElementSize;
    const auto* in = m_Content.data();

    for (std::size_t i = 0; i < n; ++i) {
        auto elem = PointElement::New();
        elem->SetName(
            fixed_string_unpack(in + 0, kLenName).c_str());
        elem->SetGroupName(
            fixed_string_unpack(in + 64, kLenGroup).c_str());
        elem->SetRGBA(in[96], in[97], in[98], in[99]);
        elem->SetPosition(
            bo::read_be_f32(in + 100),
            bo::read_be_f32(in + 104),
            bo::read_be_f32(in + 108));
        elem->SetRadius(bo::read_be_f32(in + 112));
        elem->SetOwner(
            fixed_string_unpack(in + 116, kLenOwner).c_str());
        m_PointList.push_back(elem);
        in += kElementSize;
    }
    return 1;
}

}  // namespace igtl
