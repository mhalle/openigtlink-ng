// igtlTransformMessage.h — shim facade matching upstream's
// TransformMessage API byte-for-byte on the wire.
#ifndef __igtlTransformMessage_h
#define __igtlTransformMessage_h

#include "igtlMacro.h"
#include "igtlMath.h"
#include "igtlMessageBase.h"
#include "igtlObject.h"

namespace igtl {

// GET_TRANS — header-only query message.
class IGTLCommon_EXPORT GetTransformMessage
    : public MessageBase {
 public:
    igtlTypeMacro(igtl::GetTransformMessage, igtl::MessageBase);
    igtlNewMacro(igtl::GetTransformMessage);
 protected:
    GetTransformMessage()  { m_SendMessageType = "GET_TRANS"; }
    ~GetTransformMessage() override = default;
};

// TRANSFORM — 4x4 homogeneous transform.
class IGTLCommon_EXPORT TransformMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::TransformMessage, igtl::MessageBase);
    igtlNewMacro(igtl::TransformMessage);

    // ---- position / translation (column 3 of the upper 3x3+translation block) ----
    void SetPosition(float p[3]);
    void GetPosition(float p[3]);
    void SetPosition(float px, float py, float pz);
    void GetPosition(float* px, float* py, float* pz);

    // ---- rotation (upper 3x3) ----
    void SetNormals(float o[3][3]);
    void GetNormals(float o[3][3]);
    void SetNormals(float t[3], float s[3], float n[3]);
    void GetNormals(float t[3], float s[3], float n[3]);

    // ---- full 4x4 ----
    void SetMatrix(Matrix4x4& mat);
    void GetMatrix(Matrix4x4& mat);

 protected:
    TransformMessage();
    ~TransformMessage() override;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    Matrix4x4 matrix;
};

}  // namespace igtl

#endif  // __igtlTransformMessage_h
