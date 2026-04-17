// Hand-written facade — supersedes the codegen stub.
//
// POSITION — 3D position with optional orientation quaternion.
// Wire body is exactly 12, 24, or 28 bytes (per spec/schemas/
// position.json):
//
//   POSITION_ONLY     (12 B): 3 float32 position only.
//   WITH_QUATERNION3  (24 B): 3 float32 position + 3 float32
//                             (compressed quaternion; qw implicit).
//   ALL               (28 B): 3 float32 position + 4 float32 quat.
//
// Pack mode is chosen via SetPackType() / SetPackTypeByContentSize().
// Default is ALL (28 B), matching upstream.
#ifndef __igtlPositionMessage_h
#define __igtlPositionMessage_h

#include "igtlMacro.h"
#include "igtlMessageBase.h"
#include "igtlTypes.h"

namespace igtl {

class IGTLCommon_EXPORT PositionMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::PositionMessage, igtl::MessageBase);
    igtlNewMacro(igtl::PositionMessage);

    // Pack-type enum values match upstream exactly.
    enum {
        POSITION_ONLY    = 1,
        WITH_QUATERNION3 = 2,
        ALL              = 3,
    };

    void Init();  // reset fields to defaults

    void SetPackType(int t);
    int  GetPackType() { return m_PackType; }

    // Choose pack type from a wire content size (12, 24, or 28).
    // Returns 1 on recognised size, 0 otherwise.
    int SetPackTypeByContentSize(int s);

    void SetPosition(const float* pos);
    void SetPosition(float x, float y, float z);

    void SetQuaternion(const float* quat);
    void SetQuaternion(float ox, float oy, float oz, float w);

    void GetPosition(float* pos);
    void GetPosition(float* x, float* y, float* z);

    void GetQuaternion(float* quat);
    void GetQuaternion(float* ox, float* oy, float* oz, float* w);

 protected:
    PositionMessage();
    ~PositionMessage() override;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    int         m_PackType;
    igtlFloat32 m_Position[3];
    igtlFloat32 m_Quaternion[4];
};

}  // namespace igtl

#endif  // __igtlPositionMessage_h
