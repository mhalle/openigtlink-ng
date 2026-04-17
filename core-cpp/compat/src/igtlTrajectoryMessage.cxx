// Hand-written TRAJ facade. Body: N × 150-byte elements.

#include "igtl/igtlTrajectoryMessage.h"

#include <algorithm>
#include <cstring>

#include "oigtl/runtime/byte_order.hpp"

namespace igtl {

namespace {
constexpr std::size_t kElementSize = 150;
constexpr std::size_t kLenName     = 64;
constexpr std::size_t kLenGroup    = 32;
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

// ---------------- TrajectoryElement ----------------
TrajectoryElement::TrajectoryElement()
    : m_Type(0), m_Radius(0.0f) {
    m_RGBA[0] = m_RGBA[1] = m_RGBA[2] = m_RGBA[3] = 0;
    m_EntryPos[0]  = m_EntryPos[1]  = m_EntryPos[2]  = 0.0f;
    m_TargetPos[0] = m_TargetPos[1] = m_TargetPos[2] = 0.0f;
}

int TrajectoryElement::SetName(const char* n) {
    m_Name = n ? n : "";
    return static_cast<int>(m_Name.size());
}
int TrajectoryElement::SetGroupName(const char* g) {
    m_GroupName = g ? g : "";
    return static_cast<int>(m_GroupName.size());
}
int TrajectoryElement::SetOwner(const char* o) {
    m_Owner = o ? o : "";
    return static_cast<int>(m_Owner.size());
}

void TrajectoryElement::SetRGBA(igtlUint8 rgba[4]) {
    m_RGBA[0] = rgba[0]; m_RGBA[1] = rgba[1];
    m_RGBA[2] = rgba[2]; m_RGBA[3] = rgba[3];
}
void TrajectoryElement::SetRGBA(igtlUint8 r, igtlUint8 g,
                                igtlUint8 b, igtlUint8 a) {
    m_RGBA[0] = r; m_RGBA[1] = g; m_RGBA[2] = b; m_RGBA[3] = a;
}
void TrajectoryElement::GetRGBA(igtlUint8* rgba) {
    rgba[0] = m_RGBA[0]; rgba[1] = m_RGBA[1];
    rgba[2] = m_RGBA[2]; rgba[3] = m_RGBA[3];
}

void TrajectoryElement::SetEntryPosition(igtlFloat32 p[3]) {
    m_EntryPos[0] = p[0]; m_EntryPos[1] = p[1]; m_EntryPos[2] = p[2];
}
void TrajectoryElement::SetEntryPosition(igtlFloat32 x, igtlFloat32 y,
                                          igtlFloat32 z) {
    m_EntryPos[0] = x; m_EntryPos[1] = y; m_EntryPos[2] = z;
}
void TrajectoryElement::GetEntryPosition(igtlFloat32* p) {
    p[0] = m_EntryPos[0]; p[1] = m_EntryPos[1]; p[2] = m_EntryPos[2];
}

void TrajectoryElement::SetTargetPosition(igtlFloat32 p[3]) {
    m_TargetPos[0] = p[0]; m_TargetPos[1] = p[1]; m_TargetPos[2] = p[2];
}
void TrajectoryElement::SetTargetPosition(igtlFloat32 x, igtlFloat32 y,
                                           igtlFloat32 z) {
    m_TargetPos[0] = x; m_TargetPos[1] = y; m_TargetPos[2] = z;
}
void TrajectoryElement::GetTargetPosition(igtlFloat32* p) {
    p[0] = m_TargetPos[0]; p[1] = m_TargetPos[1]; p[2] = m_TargetPos[2];
}

// ---------------- TrajectoryMessage ----------------
TrajectoryMessage::TrajectoryMessage() {
    m_SendMessageType = "TRAJ";
}

int TrajectoryMessage::AddTrajectoryElement(TrajectoryElement::Pointer& e) {
    m_TrajectoryList.push_back(e);
    return static_cast<int>(m_TrajectoryList.size());
}
void TrajectoryMessage::ClearTrajectoryElement() {
    m_TrajectoryList.clear();
}
int TrajectoryMessage::GetNumberOfTrajectoryElement() {
    return static_cast<int>(m_TrajectoryList.size());
}
void TrajectoryMessage::GetTrajectoryElement(
        int i, TrajectoryElement::Pointer& e) {
    if (i < 0 ||
        static_cast<std::size_t>(i) >= m_TrajectoryList.size()) {
        e = TrajectoryElement::Pointer();
        return;
    }
    e = m_TrajectoryList[i];
}

igtlUint64 TrajectoryMessage::CalculateContentBufferSize() {
    return m_TrajectoryList.size() * kElementSize;
}

int TrajectoryMessage::PackContent() {
    namespace bo = oigtl::runtime::byte_order;
    const std::size_t need = m_TrajectoryList.size() * kElementSize;
    if (m_Content.size() < need) m_Content.assign(need, 0);
    auto* out = m_Content.data();

    for (auto& e : m_TrajectoryList) {
        std::size_t off = 0;
        fs_pack(out + off, kLenName,  e->GetName());       off += kLenName;
        fs_pack(out + off, kLenGroup, e->GetGroupName());  off += kLenGroup;
        out[off++] = e->GetType();
        out[off++] = 0;  // reserved
        igtlUint8 rgba[4]; e->GetRGBA(rgba);
        out[off++] = rgba[0]; out[off++] = rgba[1];
        out[off++] = rgba[2]; out[off++] = rgba[3];
        igtlFloat32 p[3]; e->GetEntryPosition(p);
        bo::write_be_f32(out + off + 0, p[0]);
        bo::write_be_f32(out + off + 4, p[1]);
        bo::write_be_f32(out + off + 8, p[2]);
        off += 12;
        e->GetTargetPosition(p);
        bo::write_be_f32(out + off + 0, p[0]);
        bo::write_be_f32(out + off + 4, p[1]);
        bo::write_be_f32(out + off + 8, p[2]);
        off += 12;
        bo::write_be_f32(out + off, e->GetRadius()); off += 4;
        fs_pack(out + off, kLenOwner, e->GetOwner()); off += kLenOwner;
        (void)off;
        out += kElementSize;
    }
    return 1;
}

int TrajectoryMessage::UnpackContent() {
    namespace bo = oigtl::runtime::byte_order;
    const std::size_t sz = m_Content.size();
    if (sz % kElementSize != 0) return 0;
    ClearTrajectoryElement();
    const std::size_t n = sz / kElementSize;
    const auto* in = m_Content.data();
    for (std::size_t i = 0; i < n; ++i) {
        auto e = TrajectoryElement::New();
        std::size_t off = 0;
        e->SetName(fs_unpack(in + off, kLenName).c_str());        off += kLenName;
        e->SetGroupName(fs_unpack(in + off, kLenGroup).c_str()); off += kLenGroup;
        e->SetType(in[off]); off += 2;  // +reserved
        e->SetRGBA(in[off], in[off+1], in[off+2], in[off+3]); off += 4;
        e->SetEntryPosition(
            bo::read_be_f32(in + off + 0),
            bo::read_be_f32(in + off + 4),
            bo::read_be_f32(in + off + 8));
        off += 12;
        e->SetTargetPosition(
            bo::read_be_f32(in + off + 0),
            bo::read_be_f32(in + off + 4),
            bo::read_be_f32(in + off + 8));
        off += 12;
        e->SetRadius(bo::read_be_f32(in + off)); off += 4;
        e->SetOwner(fs_unpack(in + off, kLenOwner).c_str()); off += kLenOwner;
        (void)off;
        m_TrajectoryList.push_back(e);
        in += kElementSize;
    }
    return 1;
}

}  // namespace igtl
