// igtlMath.h — matrix helpers with the exact signatures upstream
// headers use. Implementations mirror upstream's reference.
#ifndef __igtlMath_h
#define __igtlMath_h

#include "igtlMacro.h"
#include "igtlTypes.h"

namespace igtl {

/// Load the 4x4 identity matrix into `matrix`.
IGTLCommon_EXPORT void IdentityMatrix(Matrix4x4& matrix);

/// Print a matrix to stdout in upstream's format (one row per line,
/// tab-separated). Used by Examples/Tracker and similar.
IGTLCommon_EXPORT void PrintMatrix(Matrix4x4& matrix);

/// t,s,n are the three 3-vectors along the local tracker axes; p is
/// the position. Writes the resulting 4x4 to `matrix`.
IGTLCommon_EXPORT void QuaternionToMatrix(
    float* q, Matrix4x4& m);
IGTLCommon_EXPORT void MatrixToQuaternion(
    Matrix4x4& m, float* q);

IGTLCommon_EXPORT void PrintVector3(float v[3]);
IGTLCommon_EXPORT void PrintVector3(float x, float y, float z);
IGTLCommon_EXPORT void PrintVector4(float v[4]);
IGTLCommon_EXPORT void PrintVector4(float x, float y, float z, float w);

}  // namespace igtl

#endif  // __igtlMath_h
