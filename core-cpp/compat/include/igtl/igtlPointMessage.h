// Hand-written facade — supersedes the codegen stub.
//
// POINT — list of 3D annotated points (landmarks, fiducials, etc.).
// Each element is exactly 136 bytes on the wire; body_size = 136*N.
//
// Two classes, matching upstream's shape:
//   igtl::PointElement  — one annotated point. Reference-counted
//                         LightObject subclass. Handed around via
//                         `PointElement::Pointer`.
//   igtl::PointMessage  — container with AddPointElement /
//                         GetPointElement / GetNumberOfPointElement.
#ifndef __igtlPointMessage_h
#define __igtlPointMessage_h

#include "igtlMacro.h"
#include "igtlMessageBase.h"
#include "igtlObject.h"
#include "igtlTypes.h"

#include <string>
#include <vector>

namespace igtl {

class IGTLCommon_EXPORT PointElement : public Object {
 public:
    igtlTypeMacro(igtl::PointElement, igtl::Object);
    igtlNewMacro(igtl::PointElement);

    int         SetName(const char* name);
    const char* GetName() { return m_Name.c_str(); }

    int         SetGroupName(const char* g);
    const char* GetGroupName() { return m_GroupName.c_str(); }

    void SetRGBA(igtlUint8 rgba[4]);
    void SetRGBA(igtlUint8 r, igtlUint8 g, igtlUint8 b, igtlUint8 a);
    void GetRGBA(igtlUint8* rgba);
    void GetRGBA(igtlUint8& r, igtlUint8& g, igtlUint8& b, igtlUint8& a);

    void SetPosition(igtlFloat32 pos[3]);
    void SetPosition(igtlFloat32 x, igtlFloat32 y, igtlFloat32 z);
    void GetPosition(igtlFloat32* pos);
    void GetPosition(igtlFloat32& x, igtlFloat32& y, igtlFloat32& z);

    void        SetRadius(igtlFloat32 r) { m_Radius = r; }
    igtlFloat32 GetRadius()              { return m_Radius; }

    int         SetOwner(const char* owner);
    const char* GetOwner() { return m_Owner.c_str(); }

 protected:
    PointElement();
    ~PointElement() override = default;

    std::string m_Name;
    std::string m_GroupName;
    igtlUint8   m_RGBA[4];
    igtlFloat32 m_Position[3];
    igtlFloat32 m_Radius;
    std::string m_Owner;
};

class IGTLCommon_EXPORT PointMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::PointMessage, igtl::MessageBase);
    igtlNewMacro(igtl::PointMessage);

    int  AddPointElement(PointElement::Pointer& elem);
    void ClearPointElements();
    int  GetNumberOfPointElement();
    void GetPointElement(int index, PointElement::Pointer& elem);

 protected:
    PointMessage();
    ~PointMessage() override = default;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    std::vector<PointElement::Pointer> m_PointList;
};

}  // namespace igtl

#endif  // __igtlPointMessage_h
