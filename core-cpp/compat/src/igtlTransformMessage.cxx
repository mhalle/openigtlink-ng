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
// Upstream's TRANSFORM body is exactly 48 bytes: 12 big-endian
// floats, in row-major order, covering matrix[0..2][0..3].
// (The 4th row is implicit [0,0,0,1].)
//
// IMPORTANT wire-order detail: upstream serialises in the order
//   t[0] s[0] n[0] p[0]
//   t[1] s[1] n[1] p[1]
//   t[2] s[2] n[2] p[2]
// — i.e. *row*-major over rows 0..2, with translation in column 3.
// Our `matrix[i][j]` stores element (row i, column j), so the wire
// is simply matrix[0..2][0..3] flattened row-wise. Same 12 floats
// our own oigtl::messages::Transform schema declares.

igtlUint64 TransformMessage::CalculateContentBufferSize() {
    return 48;  // 12 float32s
}

int TransformMessage::PackContent() {
    namespace bo = oigtl::runtime::byte_order;
    if (m_Content.size() < 48) m_Content.assign(48, 0);
    auto* out = m_Content.data();
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            bo::write_be_f32(out + (i * 4 + j) * 4, matrix[i][j]);
        }
    }
    return 1;
}

int TransformMessage::UnpackContent() {
    namespace bo = oigtl::runtime::byte_order;
    if (m_Content.size() < 48) return 0;
    const auto* in = m_Content.data();
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            matrix[i][j] = bo::read_be_f32(in + (i * 4 + j) * 4);
        }
    }
    matrix[3][0] = 0.0f;
    matrix[3][1] = 0.0f;
    matrix[3][2] = 0.0f;
    matrix[3][3] = 1.0f;
    return 1;
}

}  // namespace igtl
