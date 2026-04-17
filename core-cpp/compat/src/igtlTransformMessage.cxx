// igtlTransformMessage.cxx — pack/unpack the 12-float upper-3x4
// block of a 4x4 matrix in big-endian float32s. Matches upstream's
// wire layout byte-for-byte.

#include "igtl/igtlTransformMessage.h"

#include <cstring>

#include "oigtl/runtime/byte_order.hpp"

namespace igtl {

TransformMessage::TransformMessage() {
    m_SendMessageType = "TRANSFORM";
    IdentityMatrix(matrix);
}

TransformMessage::~TransformMessage() = default;

// ---- 4x4 set/get ----
void TransformMessage::SetMatrix(Matrix4x4& m) {
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            matrix[i][j] = m[i][j];
}

void TransformMessage::GetMatrix(Matrix4x4& m) {
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            m[i][j] = matrix[i][j];
}

// ---- position (column 3, rows 0..2) ----
void TransformMessage::SetPosition(float p[3]) {
    matrix[0][3] = p[0]; matrix[1][3] = p[1]; matrix[2][3] = p[2];
}
void TransformMessage::GetPosition(float p[3]) {
    p[0] = matrix[0][3]; p[1] = matrix[1][3]; p[2] = matrix[2][3];
}
void TransformMessage::SetPosition(float px, float py, float pz) {
    matrix[0][3] = px; matrix[1][3] = py; matrix[2][3] = pz;
}
void TransformMessage::GetPosition(float* px, float* py, float* pz) {
    *px = matrix[0][3]; *py = matrix[1][3]; *pz = matrix[2][3];
}

// ---- normals (upper 3x3) ----
// Upstream stores the orientation as three column vectors t/s/n.
// The 3x3 block is column-major in the t/s/n convention:
//   matrix[i][0] = t[i], matrix[i][1] = s[i], matrix[i][2] = n[i]
void TransformMessage::SetNormals(float o[3][3]) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            matrix[i][j] = o[i][j];
}
void TransformMessage::GetNormals(float o[3][3]) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            o[i][j] = matrix[i][j];
}
void TransformMessage::SetNormals(float t[3], float s[3], float n[3]) {
    for (int i = 0; i < 3; ++i) {
        matrix[i][0] = t[i];
        matrix[i][1] = s[i];
        matrix[i][2] = n[i];
    }
}
void TransformMessage::GetNormals(float t[3], float s[3], float n[3]) {
    for (int i = 0; i < 3; ++i) {
        t[i] = matrix[i][0];
        s[i] = matrix[i][1];
        n[i] = matrix[i][2];
    }
}

// ---- wire layout ----
// Upstream's TRANSFORM body is 48 bytes — 12 big-endian floats
// in **column-major 3x4** order. The 12 floats decompose as
//   [ column 0 = R_col0 : matrix[0..2][0] ]
//   [ column 1 = R_col1 : matrix[0..2][1] ]
//   [ column 2 = R_col2 : matrix[0..2][2] ]
//   [ column 3 = t      : matrix[0..2][3] = Tx Ty Tz ]
// which is the rotation's three column vectors followed by the
// translation vector. The 4th row of matrix is implicit [0,0,0,1]
// and never on the wire. Matches our own oigtl::messages::Transform
// schema (`"layout": "column_major_3x4"`) and upstream's
// TransformMessage::PackContent byte-for-byte.

igtlUint64 TransformMessage::CalculateContentBufferSize() {
    return 48;  // 12 float32s
}

int TransformMessage::PackContent() {
    namespace bo = oigtl::runtime::byte_order;
    if (m_Content.size() < 48) m_Content.assign(48, 0);
    auto* out = m_Content.data();
    std::size_t pos = 0;
    // Column-major: outer loop over columns, inner over rows.
    for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 3; ++i) {
            bo::write_be_f32(out + pos, matrix[i][j]);
            pos += 4;
        }
    }
    return 1;
}

int TransformMessage::UnpackContent() {
    namespace bo = oigtl::runtime::byte_order;
    if (m_Content.size() < 48) return 0;
    const auto* in = m_Content.data();
    std::size_t pos = 0;
    for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 3; ++i) {
            matrix[i][j] = bo::read_be_f32(in + pos);
            pos += 4;
        }
    }
    matrix[3][0] = 0.0f;
    matrix[3][1] = 0.0f;
    matrix[3][2] = 0.0f;
    matrix[3][3] = 1.0f;
    return 1;
}

}  // namespace igtl
