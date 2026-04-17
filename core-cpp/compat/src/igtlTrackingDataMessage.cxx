// Hand-written TDATA facade. Body is N × 70-byte elements.

#include "igtl/igtlTrackingDataMessage.h"

#include <algorithm>
#include <cstring>

#include "oigtl/runtime/byte_order.hpp"

namespace igtl {

namespace {
constexpr std::size_t kElementSize = 70;
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

// Column-major 3x4 packer — same layout as TRANSFORM.
void pack_matrix_cm34(std::uint8_t* out, const Matrix4x4& m) {
    namespace bo = oigtl::runtime::byte_order;
    std::size_t pos = 0;
    for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 3; ++i) {
            bo::write_be_f32(out + pos, m[i][j]);
            pos += 4;
        }
    }
}

void unpack_matrix_cm34(const std::uint8_t* in, Matrix4x4& m) {
    namespace bo = oigtl::runtime::byte_order;
    std::size_t pos = 0;
    for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 3; ++i) {
            m[i][j] = bo::read_be_f32(in + pos);
            pos += 4;
        }
    }
    m[3][0] = m[3][1] = m[3][2] = 0.0f;
    m[3][3] = 1.0f;
}
}  // namespace

// ---------------- TrackingDataElement ----------------
TrackingDataElement::TrackingDataElement()
    : m_Type(TYPE_TRACKER) {
    IdentityMatrix(m_Matrix);
}

int TrackingDataElement::SetName(const char* n) {
    m_Name = n ? n : "";
    return static_cast<int>(m_Name.size());
}

int TrackingDataElement::SetType(igtlUint8 t) {
    m_Type = t;
    return 1;
}

void TrackingDataElement::SetPosition(float p[3]) {
    m_Matrix[0][3] = p[0];
    m_Matrix[1][3] = p[1];
    m_Matrix[2][3] = p[2];
}
void TrackingDataElement::SetPosition(float x, float y, float z) {
    m_Matrix[0][3] = x;
    m_Matrix[1][3] = y;
    m_Matrix[2][3] = z;
}
void TrackingDataElement::GetPosition(float p[3]) {
    p[0] = m_Matrix[0][3];
    p[1] = m_Matrix[1][3];
    p[2] = m_Matrix[2][3];
}
void TrackingDataElement::GetPosition(float* x, float* y, float* z) {
    *x = m_Matrix[0][3];
    *y = m_Matrix[1][3];
    *z = m_Matrix[2][3];
}

void TrackingDataElement::SetMatrix(Matrix4x4& m) {
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            m_Matrix[i][j] = m[i][j];
}
void TrackingDataElement::GetMatrix(Matrix4x4& m) {
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            m[i][j] = m_Matrix[i][j];
}

// ---------------- TrackingDataMessage ----------------
TrackingDataMessage::TrackingDataMessage() {
    m_SendMessageType = "TDATA";
}

int TrackingDataMessage::AddTrackingDataElement(
        TrackingDataElement::Pointer& elem) {
    m_TrackingDataList.push_back(elem);
    return static_cast<int>(m_TrackingDataList.size());
}

void TrackingDataMessage::ClearTrackingDataElements() {
    m_TrackingDataList.clear();
}

int TrackingDataMessage::GetNumberOfTrackingDataElements() {
    return static_cast<int>(m_TrackingDataList.size());
}

void TrackingDataMessage::GetTrackingDataElement(
        int index, TrackingDataElement::Pointer& elem) {
    if (index < 0 ||
        static_cast<std::size_t>(index) >= m_TrackingDataList.size()) {
        elem = TrackingDataElement::Pointer();
        return;
    }
    elem = m_TrackingDataList[index];
}

igtlUint64 TrackingDataMessage::CalculateContentBufferSize() {
    return m_TrackingDataList.size() * kElementSize;
}

int TrackingDataMessage::PackContent() {
    const std::size_t need = m_TrackingDataList.size() * kElementSize;
    if (m_Content.size() < need) m_Content.assign(need, 0);
    auto* out = m_Content.data();

    for (auto& elem : m_TrackingDataList) {
        fixed_string_pack(out + 0, kLenName, elem->GetName());
        out[20] = elem->GetType();
        out[21] = 0;  // reserved
        Matrix4x4 m;
        elem->GetMatrix(m);
        pack_matrix_cm34(out + 22, m);
        out += kElementSize;
    }
    return 1;
}

int TrackingDataMessage::UnpackContent() {
    const std::size_t sz = m_Content.size();
    if (sz % kElementSize != 0) return 0;

    ClearTrackingDataElements();
    const std::size_t n = sz / kElementSize;
    const auto* in = m_Content.data();

    for (std::size_t i = 0; i < n; ++i) {
        auto elem = TrackingDataElement::New();
        elem->SetName(
            fixed_string_unpack(in + 0, kLenName).c_str());
        elem->SetType(in[20]);
        // byte 21 is reserved — ignored
        Matrix4x4 m;
        unpack_matrix_cm34(in + 22, m);
        elem->SetMatrix(m);
        m_TrackingDataList.push_back(elem);
        in += kElementSize;
    }
    return 1;
}

}  // namespace igtl
