// Hand-written facade — supersedes the codegen stub.
//
// TDATA — list of tracked tools. Each per-element wire record is
// exactly 70 bytes (per spec/schemas/tdata.json):
//   20 B  name (null-padded ASCII)
//    1 B  type (TYPE_TRACKER=1, TYPE_6D=2, TYPE_3D=3, TYPE_5D=4)
//    1 B  reserved (SHOULD be 0)
//   48 B  transform (12 float32, column-major 3x4 same as TRANSFORM)
//
// Two classes match upstream exactly:
//   igtl::TrackingDataElement   — one tool, LightObject subclass.
//   igtl::TrackingDataMessage   — container with
//                                 AddTrackingDataElement / Get / etc.
#ifndef __igtlTrackingDataMessage_h
#define __igtlTrackingDataMessage_h

#include "igtlMacro.h"
#include "igtlMath.h"
#include "igtlMessageBase.h"
#include "igtlObject.h"
#include "igtlTypes.h"

#include <string>
#include <vector>

namespace igtl {

class IGTLCommon_EXPORT TrackingDataElement : public Object {
 public:
    igtlTypeMacro(igtl::TrackingDataElement, igtl::Object);
    igtlNewMacro(igtl::TrackingDataElement);

    enum {
        TYPE_TRACKER = 1,  // reference tracker
        TYPE_6D      = 2,  // 6 DoF instrument
        TYPE_3D      = 3,  // 3 DoF (tip only)
        TYPE_5D      = 4,  // 5 DoF (tip + handle)
    };

    int         SetName(const char* n);
    const char* GetName() { return m_Name.c_str(); }

    int       SetType(igtlUint8 t);
    igtlUint8 GetType() { return m_Type; }

    void SetPosition(float p[3]);
    void SetPosition(float x, float y, float z);
    void GetPosition(float p[3]);
    void GetPosition(float* x, float* y, float* z);

    void SetMatrix(Matrix4x4& m);
    void GetMatrix(Matrix4x4& m);

 protected:
    TrackingDataElement();
    ~TrackingDataElement() override = default;

    std::string m_Name;
    igtlUint8   m_Type;
    Matrix4x4   m_Matrix;
};

class IGTLCommon_EXPORT TrackingDataMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::TrackingDataMessage, igtl::MessageBase);
    igtlNewMacro(igtl::TrackingDataMessage);

    int  AddTrackingDataElement(TrackingDataElement::Pointer& elem);
    void ClearTrackingDataElements();
    int  GetNumberOfTrackingDataElements();
    // Upstream kept the typo-free alias for API migration.
    int  GetNumberOfTrackingDataElement() {
        return GetNumberOfTrackingDataElements();
    }
    void GetTrackingDataElement(int index,
                                TrackingDataElement::Pointer& elem);

 protected:
    TrackingDataMessage();
    ~TrackingDataMessage() override = default;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    std::vector<TrackingDataElement::Pointer> m_TrackingDataList;
};

}  // namespace igtl

#endif  // __igtlTrackingDataMessage_h
