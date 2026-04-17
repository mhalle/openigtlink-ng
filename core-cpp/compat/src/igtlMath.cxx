// igtlMath.cxx — matrix helpers. Formulas match upstream's
// reference.

#include "igtl/igtlMath.h"

#include <cmath>
#include <cstdio>

namespace igtl {

void IdentityMatrix(Matrix4x4& m) {
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            m[i][j] = (i == j) ? 1.0f : 0.0f;
}

void PrintMatrix(Matrix4x4& m) {
    for (int i = 0; i < 4; ++i) {
        std::printf("%f\t%f\t%f\t%f\n",
                    m[i][0], m[i][1], m[i][2], m[i][3]);
    }
}

// Quaternion ↔ 3x3 rotation block of a 4x4. Translation untouched.
void QuaternionToMatrix(float* q, Matrix4x4& m) {
    const float qx = q[0], qy = q[1], qz = q[2], qw = q[3];
    const float xx = qx * qx, yy = qy * qy, zz = qz * qz;
    const float xy = qx * qy, xz = qx * qz, yz = qy * qz;
    const float wx = qw * qx, wy = qw * qy, wz = qw * qz;
    m[0][0] = 1.0f - 2.0f * (yy + zz);
    m[0][1] =        2.0f * (xy - wz);
    m[0][2] =        2.0f * (xz + wy);
    m[1][0] =        2.0f * (xy + wz);
    m[1][1] = 1.0f - 2.0f * (xx + zz);
    m[1][2] =        2.0f * (yz - wx);
    m[2][0] =        2.0f * (xz - wy);
    m[2][1] =        2.0f * (yz + wx);
    m[2][2] = 1.0f - 2.0f * (xx + yy);
}

void MatrixToQuaternion(Matrix4x4& m, float* q) {
    const float tr = m[0][0] + m[1][1] + m[2][2];
    if (tr > 0.0f) {
        const float s = std::sqrt(tr + 1.0f) * 2.0f;
        q[3] = 0.25f * s;
        q[0] = (m[2][1] - m[1][2]) / s;
        q[1] = (m[0][2] - m[2][0]) / s;
        q[2] = (m[1][0] - m[0][1]) / s;
    } else if ((m[0][0] > m[1][1]) && (m[0][0] > m[2][2])) {
        const float s = std::sqrt(
            1.0f + m[0][0] - m[1][1] - m[2][2]) * 2.0f;
        q[3] = (m[2][1] - m[1][2]) / s;
        q[0] = 0.25f * s;
        q[1] = (m[0][1] + m[1][0]) / s;
        q[2] = (m[0][2] + m[2][0]) / s;
    } else if (m[1][1] > m[2][2]) {
        const float s = std::sqrt(
            1.0f + m[1][1] - m[0][0] - m[2][2]) * 2.0f;
        q[3] = (m[0][2] - m[2][0]) / s;
        q[0] = (m[0][1] + m[1][0]) / s;
        q[1] = 0.25f * s;
        q[2] = (m[1][2] + m[2][1]) / s;
    } else {
        const float s = std::sqrt(
            1.0f + m[2][2] - m[0][0] - m[1][1]) * 2.0f;
        q[3] = (m[1][0] - m[0][1]) / s;
        q[0] = (m[0][2] + m[2][0]) / s;
        q[1] = (m[1][2] + m[2][1]) / s;
        q[2] = 0.25f * s;
    }
}

}  // namespace igtl
