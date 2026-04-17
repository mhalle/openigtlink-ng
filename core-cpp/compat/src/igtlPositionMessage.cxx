// Hand-written PositionMessage facade. Body-size dispatches on
// the `pack_type` mode; POSITION_ONLY omits orientation,
// WITH_QUATERNION3 compresses to 3 quaternion components, ALL
// carries the full 4-component quaternion.

#include "igtl/igtlPositionMessage.h"

#include <cmath>
#include <cstring>

#include "oigtl/runtime/byte_order.hpp"

namespace igtl {

PositionMessage::PositionMessage()
    : m_PackType(ALL) {
    m_SendMessageType = "POSITION";
    Init();
}

PositionMessage::~PositionMessage() = default;

void PositionMessage::Init() {
    m_Position[0] = m_Position[1] = m_Position[2] = 0.0f;
    m_Quaternion[0] = m_Quaternion[1] = m_Quaternion[2] = 0.0f;
    m_Quaternion[3] = 1.0f;
}

void PositionMessage::SetPackType(int t) {
    if (t == POSITION_ONLY || t == WITH_QUATERNION3 || t == ALL) {
        m_PackType = t;
    }
}

int PositionMessage::SetPackTypeByContentSize(int s) {
    switch (s) {
        case 12: m_PackType = POSITION_ONLY;    return 1;
        case 24: m_PackType = WITH_QUATERNION3; return 1;
        case 28: m_PackType = ALL;              return 1;
        default:                                return 0;
    }
}

void PositionMessage::SetPosition(const float* p) {
    m_Position[0] = p[0]; m_Position[1] = p[1]; m_Position[2] = p[2];
}
void PositionMessage::SetPosition(float x, float y, float z) {
    m_Position[0] = x; m_Position[1] = y; m_Position[2] = z;
}
void PositionMessage::SetQuaternion(const float* q) {
    m_Quaternion[0] = q[0]; m_Quaternion[1] = q[1];
    m_Quaternion[2] = q[2]; m_Quaternion[3] = q[3];
}
void PositionMessage::SetQuaternion(float ox, float oy, float oz, float w) {
    m_Quaternion[0] = ox; m_Quaternion[1] = oy;
    m_Quaternion[2] = oz; m_Quaternion[3] = w;
}

void PositionMessage::GetPosition(float* p) {
    p[0] = m_Position[0]; p[1] = m_Position[1]; p[2] = m_Position[2];
}
void PositionMessage::GetPosition(float* x, float* y, float* z) {
    *x = m_Position[0]; *y = m_Position[1]; *z = m_Position[2];
}
void PositionMessage::GetQuaternion(float* q) {
    q[0] = m_Quaternion[0]; q[1] = m_Quaternion[1];
    q[2] = m_Quaternion[2]; q[3] = m_Quaternion[3];
}
void PositionMessage::GetQuaternion(float* ox, float* oy, float* oz, float* w) {
    *ox = m_Quaternion[0]; *oy = m_Quaternion[1];
    *oz = m_Quaternion[2]; *w  = m_Quaternion[3];
}

// ---- wire packing ----
igtlUint64 PositionMessage::CalculateContentBufferSize() {
    switch (m_PackType) {
        case POSITION_ONLY:    return 12;
        case WITH_QUATERNION3: return 24;
        case ALL:              return 28;
        default:               return 28;
    }
}

int PositionMessage::PackContent() {
    namespace bo = oigtl::runtime::byte_order;
    const std::size_t need = static_cast<std::size_t>(
        CalculateContentBufferSize());
    if (m_Content.size() < need) m_Content.assign(need, 0);
    auto* out = m_Content.data();

    // Position is always 12 bytes.
    bo::write_be_f32(out + 0, m_Position[0]);
    bo::write_be_f32(out + 4, m_Position[1]);
    bo::write_be_f32(out + 8, m_Position[2]);

    if (m_PackType == POSITION_ONLY) return 1;

    // Quaternion components 0..2 (compressed or full).
    bo::write_be_f32(out + 12, m_Quaternion[0]);
    bo::write_be_f32(out + 16, m_Quaternion[1]);
    bo::write_be_f32(out + 20, m_Quaternion[2]);

    if (m_PackType == WITH_QUATERNION3) return 1;

    // Full quaternion — 4th component.
    bo::write_be_f32(out + 24, m_Quaternion[3]);
    return 1;
}

int PositionMessage::UnpackContent() {
    namespace bo = oigtl::runtime::byte_order;
    const std::size_t sz = m_Content.size();

    // Validate body-size belongs to the recognised set {12,24,28}.
    if (sz != 12 && sz != 24 && sz != 28) return 0;
    SetPackTypeByContentSize(static_cast<int>(sz));
    const auto* in = m_Content.data();

    m_Position[0] = bo::read_be_f32(in + 0);
    m_Position[1] = bo::read_be_f32(in + 4);
    m_Position[2] = bo::read_be_f32(in + 8);

    if (sz == 12) {
        // Reset orientation to identity for POSITION_ONLY.
        m_Quaternion[0] = m_Quaternion[1] = m_Quaternion[2] = 0.0f;
        m_Quaternion[3] = 1.0f;
        return 1;
    }

    m_Quaternion[0] = bo::read_be_f32(in + 12);
    m_Quaternion[1] = bo::read_be_f32(in + 16);
    m_Quaternion[2] = bo::read_be_f32(in + 20);

    if (sz == 24) {
        // Compressed — reconstruct qw from unit-norm constraint.
        const float s2 = m_Quaternion[0] * m_Quaternion[0] +
                         m_Quaternion[1] * m_Quaternion[1] +
                         m_Quaternion[2] * m_Quaternion[2];
        const float r = 1.0f - s2;
        m_Quaternion[3] = r > 0.0f ? std::sqrt(r) : 0.0f;
        return 1;
    }

    m_Quaternion[3] = bo::read_be_f32(in + 24);
    return 1;
}

}  // namespace igtl
