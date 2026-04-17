// Hand-written QTDATA facade. Body is N × 50-byte elements.

#include "igtl/igtlQuaternionTrackingDataMessage.h"

#include <algorithm>
#include <cstring>

#include "oigtl/runtime/byte_order.hpp"

namespace igtl {

namespace {
constexpr std::size_t kElementSize = 50;
constexpr std::size_t kLenName     = 20;

void fixed_string_pack(std::uint8_t* dst, std::size_t n,
                       const std::string& src) {
    std::memset(dst, 0, n);
    const std::size_t len = std::min(n, src.size());
    if (len > 0) std::memcpy(dst, src.data(), len);
}

std::string fixed_string_unpack(const std::uint8_t* src,
                                std::size_t n) {
    std::size_t len = 0;
    while (len < n && src[len] != '\0') ++len;
    return std::string(reinterpret_cast<const char*>(src), len);
}
}  // namespace

// ---------------- QuaternionTrackingDataElement ----------------
QuaternionTrackingDataElement::QuaternionTrackingDataElement()
    : m_Type(TYPE_TRACKER) {
    m_Position[0] = m_Position[1] = m_Position[2] = 0.0f;
    m_Quaternion[0] = m_Quaternion[1] = m_Quaternion[2] = 0.0f;
    m_Quaternion[3] = 1.0f;
}

int QuaternionTrackingDataElement::SetName(const char* n) {
    m_Name = n ? n : "";
    return static_cast<int>(m_Name.size());
}

int QuaternionTrackingDataElement::SetType(igtlUint8 t) {
    m_Type = t; return 1;
}

void QuaternionTrackingDataElement::SetPosition(float p[3]) {
    m_Position[0] = p[0]; m_Position[1] = p[1]; m_Position[2] = p[2];
}
void QuaternionTrackingDataElement::SetPosition(float x, float y, float z) {
    m_Position[0] = x; m_Position[1] = y; m_Position[2] = z;
}
void QuaternionTrackingDataElement::GetPosition(float p[3]) {
    p[0] = m_Position[0]; p[1] = m_Position[1]; p[2] = m_Position[2];
}
void QuaternionTrackingDataElement::GetPosition(float* x, float* y, float* z) {
    *x = m_Position[0]; *y = m_Position[1]; *z = m_Position[2];
}

void QuaternionTrackingDataElement::SetQuaternion(float q[4]) {
    m_Quaternion[0] = q[0]; m_Quaternion[1] = q[1];
    m_Quaternion[2] = q[2]; m_Quaternion[3] = q[3];
}
void QuaternionTrackingDataElement::SetQuaternion(
        float qx, float qy, float qz, float w) {
    m_Quaternion[0] = qx; m_Quaternion[1] = qy;
    m_Quaternion[2] = qz; m_Quaternion[3] = w;
}
void QuaternionTrackingDataElement::GetQuaternion(float q[4]) {
    q[0] = m_Quaternion[0]; q[1] = m_Quaternion[1];
    q[2] = m_Quaternion[2]; q[3] = m_Quaternion[3];
}
void QuaternionTrackingDataElement::GetQuaternion(
        float* qx, float* qy, float* qz, float* w) {
    *qx = m_Quaternion[0]; *qy = m_Quaternion[1];
    *qz = m_Quaternion[2]; *w  = m_Quaternion[3];
}

// ---------------- QuaternionTrackingDataMessage ----------------
QuaternionTrackingDataMessage::QuaternionTrackingDataMessage() {
    m_SendMessageType = "QTDATA";
}

int QuaternionTrackingDataMessage::AddQuaternionTrackingDataElement(
        QuaternionTrackingDataElement::Pointer& elem) {
    m_QuaternionTrackingDataList.push_back(elem);
    return static_cast<int>(m_QuaternionTrackingDataList.size());
}

void QuaternionTrackingDataMessage::ClearQuaternionTrackingDataElements() {
    m_QuaternionTrackingDataList.clear();
}

int QuaternionTrackingDataMessage::GetNumberOfQuaternionTrackingDataElements() {
    return static_cast<int>(m_QuaternionTrackingDataList.size());
}

void QuaternionTrackingDataMessage::GetQuaternionTrackingDataElement(
        int index,
        QuaternionTrackingDataElement::Pointer& elem) {
    if (index < 0 || static_cast<std::size_t>(index) >=
                     m_QuaternionTrackingDataList.size()) {
        elem = QuaternionTrackingDataElement::Pointer();
        return;
    }
    elem = m_QuaternionTrackingDataList[index];
}

igtlUint64 QuaternionTrackingDataMessage::CalculateContentBufferSize() {
    return m_QuaternionTrackingDataList.size() * kElementSize;
}

int QuaternionTrackingDataMessage::PackContent() {
    namespace bo = oigtl::runtime::byte_order;
    const std::size_t need =
        m_QuaternionTrackingDataList.size() * kElementSize;
    if (m_Content.size() < need) m_Content.assign(need, 0);
    auto* out = m_Content.data();

    for (auto& elem : m_QuaternionTrackingDataList) {
        fixed_string_pack(out + 0, kLenName, elem->GetName());
        out[20] = elem->GetType();
        out[21] = 0;  // reserved

        float p[3]; elem->GetPosition(p);
        bo::write_be_f32(out + 22, p[0]);
        bo::write_be_f32(out + 26, p[1]);
        bo::write_be_f32(out + 30, p[2]);

        float q[4]; elem->GetQuaternion(q);
        bo::write_be_f32(out + 34, q[0]);
        bo::write_be_f32(out + 38, q[1]);
        bo::write_be_f32(out + 42, q[2]);
        bo::write_be_f32(out + 46, q[3]);

        out += kElementSize;
    }
    return 1;
}

int QuaternionTrackingDataMessage::UnpackContent() {
    namespace bo = oigtl::runtime::byte_order;
    const std::size_t sz = m_Content.size();
    if (sz % kElementSize != 0) return 0;

    ClearQuaternionTrackingDataElements();
    const std::size_t n = sz / kElementSize;
    const auto* in = m_Content.data();

    for (std::size_t i = 0; i < n; ++i) {
        auto elem = QuaternionTrackingDataElement::New();
        elem->SetName(
            fixed_string_unpack(in + 0, kLenName).c_str());
        elem->SetType(in[20]);
        elem->SetPosition(
            bo::read_be_f32(in + 22),
            bo::read_be_f32(in + 26),
            bo::read_be_f32(in + 30));
        elem->SetQuaternion(
            bo::read_be_f32(in + 34),
            bo::read_be_f32(in + 38),
            bo::read_be_f32(in + 42),
            bo::read_be_f32(in + 46));
        m_QuaternionTrackingDataList.push_back(elem);
        in += kElementSize;
    }
    return 1;
}

}  // namespace igtl
