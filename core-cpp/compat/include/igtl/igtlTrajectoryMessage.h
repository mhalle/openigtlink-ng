// Hand-written facade — supersedes the codegen stub.
// TRAJ — list of 150-byte trajectory records.
#ifndef __igtlTrajectoryMessage_h
#define __igtlTrajectoryMessage_h

#include "igtlMacro.h"
#include "igtlMessageBase.h"
#include "igtlObject.h"
#include "igtlTypes.h"

#include <string>
#include <vector>

namespace igtl {

class IGTLCommon_EXPORT TrajectoryElement : public Object {
 public:
    igtlTypeMacro(igtl::TrajectoryElement, igtl::Object);
    igtlNewMacro(igtl::TrajectoryElement);

    enum {
        TYPE_ENTRY_ONLY       = 1,
        TYPE_TARGET_ONLY      = 2,
        TYPE_ENTRY_AND_TARGET = 3,
    };

    int         SetName(const char* n);
    const char* GetName() { return m_Name.c_str(); }
    int         SetGroupName(const char* g);
    const char* GetGroupName() { return m_GroupName.c_str(); }
    int         SetType(igtlUint8 t) { m_Type = t; return 1; }
    igtlUint8   GetType() { return m_Type; }

    void SetRGBA(igtlUint8 rgba[4]);
    void SetRGBA(igtlUint8 r, igtlUint8 g, igtlUint8 b, igtlUint8 a);
    void GetRGBA(igtlUint8* rgba);

    void SetEntryPosition(igtlFloat32 p[3]);
    void SetEntryPosition(igtlFloat32 x, igtlFloat32 y, igtlFloat32 z);
    void GetEntryPosition(igtlFloat32* p);

    void SetTargetPosition(igtlFloat32 p[3]);
    void SetTargetPosition(igtlFloat32 x, igtlFloat32 y, igtlFloat32 z);
    void GetTargetPosition(igtlFloat32* p);

    void        SetRadius(igtlFloat32 r) { m_Radius = r; }
    igtlFloat32 GetRadius()              { return m_Radius; }

    int         SetOwner(const char* o);
    const char* GetOwner() { return m_Owner.c_str(); }

 protected:
    TrajectoryElement();
    ~TrajectoryElement() override = default;

    std::string m_Name;
    std::string m_GroupName;
    igtlUint8   m_Type;
    igtlUint8   m_RGBA[4];
    igtlFloat32 m_EntryPos[3];
    igtlFloat32 m_TargetPos[3];
    igtlFloat32 m_Radius;
    std::string m_Owner;
};

class IGTLCommon_EXPORT TrajectoryMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::TrajectoryMessage, igtl::MessageBase);
    igtlNewMacro(igtl::TrajectoryMessage);

    int  AddTrajectoryElement(TrajectoryElement::Pointer& e);
    void ClearTrajectoryElement();
    int  GetNumberOfTrajectoryElement();
    void GetTrajectoryElement(int i, TrajectoryElement::Pointer& e);

 protected:
    TrajectoryMessage();
    ~TrajectoryMessage() override = default;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    std::vector<TrajectoryElement::Pointer> m_TrajectoryList;
};

}  // namespace igtl

#endif  // __igtlTrajectoryMessage_h
