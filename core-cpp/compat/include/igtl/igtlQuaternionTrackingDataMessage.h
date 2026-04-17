// Hand-written facade — supersedes the codegen stub.
//
// QTDATA — list of tracked tools using quaternion+position instead
// of a 4x4 matrix. Each per-element wire record is exactly 50 bytes
// (per spec/schemas/qtdata.json):
//   20 B  name (null-padded ASCII)
//    1 B  type (TYPE_TRACKER=1, TYPE_6D=2, TYPE_3D=3, TYPE_5D=4)
//    1 B  reserved
//   12 B  position  float32[3] big-endian
//   16 B  quaternion float32[4] big-endian (qx, qy, qz, qw)
//
// Same container shape as TDATA; only the per-element payload differs.
#ifndef __igtlQuaternionTrackingDataMessage_h
#define __igtlQuaternionTrackingDataMessage_h

#include "igtlMacro.h"
#include "igtlMessageBase.h"
#include "igtlObject.h"
#include "igtlTypes.h"

#include <string>
#include <vector>

namespace igtl {

class IGTLCommon_EXPORT QuaternionTrackingDataElement : public Object {
 public:
    igtlTypeMacro(igtl::QuaternionTrackingDataElement, igtl::Object);
    igtlNewMacro(igtl::QuaternionTrackingDataElement);

    enum {
        TYPE_TRACKER = 1,
        TYPE_6D      = 2,
        TYPE_3D      = 3,
        TYPE_5D      = 4,
    };

    int         SetName(const char* n);
    const char* GetName() { return m_Name.c_str(); }

    int       SetType(igtlUint8 t);
    igtlUint8 GetType() { return m_Type; }

    void SetPosition(float p[3]);
    void SetPosition(float x, float y, float z);
    void GetPosition(float p[3]);
    void GetPosition(float* x, float* y, float* z);

    void SetQuaternion(float q[4]);
    void SetQuaternion(float qx, float qy, float qz, float w);
    void GetQuaternion(float q[4]);
    void GetQuaternion(float* qx, float* qy, float* qz, float* w);

 protected:
    QuaternionTrackingDataElement();
    ~QuaternionTrackingDataElement() override = default;

    std::string m_Name;
    igtlUint8   m_Type;
    float       m_Position[3];
    float       m_Quaternion[4];
};

class IGTLCommon_EXPORT QuaternionTrackingDataMessage
        : public MessageBase {
 public:
    igtlTypeMacro(igtl::QuaternionTrackingDataMessage,
                  igtl::MessageBase);
    igtlNewMacro(igtl::QuaternionTrackingDataMessage);

    int  AddQuaternionTrackingDataElement(
            QuaternionTrackingDataElement::Pointer& elem);
    void ClearQuaternionTrackingDataElements();
    int  GetNumberOfQuaternionTrackingDataElements();
    void GetQuaternionTrackingDataElement(
            int index,
            QuaternionTrackingDataElement::Pointer& elem);

 protected:
    QuaternionTrackingDataMessage();
    ~QuaternionTrackingDataMessage() override = default;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    std::vector<QuaternionTrackingDataElement::Pointer>
        m_QuaternionTrackingDataList;
};

}  // namespace igtl

#endif  // __igtlQuaternionTrackingDataMessage_h
