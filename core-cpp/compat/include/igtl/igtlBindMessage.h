// Hand-written facade — supersedes the codegen stub.
//
// BIND — multi-message container.
// Wire body:
//    2 B  ncmessages
//  N×20 B header entries (type_id[12] + body_size[8])
//    2 B  nametable_size
//  nametable_size bytes  concatenated null-terminated device names
//                        (padded to 2-byte alignment)
//  each child body, each padded to even length
//
// Class hierarchy matches upstream:
//   BindMessageBase  — common storage + header/name-table packing
//   BindMessage      — adds GetChildMessage(i, MessageBase*)
//   GetBindMessage   — uses only the header table (no bodies)
//   StartBindMessage — GetBindMessage + u64 Resolution
//   StopBindMessage  — header-only
//   RTSBindMessage   — 1-byte Status
#ifndef __igtlBindMessage_h
#define __igtlBindMessage_h

#include "igtlMacro.h"
#include "igtlMessageBase.h"
#include "igtlTypes.h"

#include <string>
#include <vector>

namespace igtl {

class IGTLCommon_EXPORT BindMessageBase : public MessageBase {
 public:
    igtlTypeMacro(igtl::BindMessageBase, igtl::MessageBase);
    igtlNewMacro(igtl::BindMessageBase);

    void Init();

    int         SetNumberOfChildMessages(unsigned int n);
    int         GetNumberOfChildMessages();

    int         AppendChildMessage(MessageBase* child);
    int         SetChildMessage(unsigned int i, MessageBase* child);
    const char* GetChildMessageType(unsigned int i);

 protected:
    BindMessageBase();
    ~BindMessageBase() override = default;

    struct ChildMessageInfo {
        std::string               type;
        std::string               name;
        std::vector<std::uint8_t> body;
    };
    std::vector<ChildMessageInfo> m_ChildMessages;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    // Shared with GET_BIND, which emits header table + name table
    // but no bodies.
    int pack_impl(bool with_bodies);
    int unpack_impl(bool with_bodies);
};

class IGTLCommon_EXPORT BindMessage : public BindMessageBase {
 public:
    igtlTypeMacro(igtl::BindMessage, igtl::BindMessageBase);
    igtlNewMacro(igtl::BindMessage);

    // Upstream fills `child`'s m_Body from the stored bytes. We
    // copy into `child`'s m_Content via SetMessageHeader-style
    // access; a full implementation requires friend access. For
    // now the stored bytes are retrievable via GetChildMessageType
    // / GetChildBody. (Adjust as consumers require.)
    int GetChildMessage(unsigned int i, MessageBase* child);

    const std::vector<std::uint8_t>*
    GetChildBody(unsigned int i) const;

 protected:
    BindMessage();
    ~BindMessage() override = default;
};

class IGTLCommon_EXPORT GetBindMessage : public BindMessageBase {
 public:
    igtlTypeMacro(igtl::GetBindMessage, igtl::BindMessageBase);
    igtlNewMacro(igtl::GetBindMessage);

 protected:
    GetBindMessage();
    ~GetBindMessage() override = default;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;
};

class IGTLCommon_EXPORT StartBindMessage : public GetBindMessage {
 public:
    igtlTypeMacro(igtl::StartBindMessage, igtl::GetBindMessage);
    igtlNewMacro(igtl::StartBindMessage);

    void       SetResolution(igtlUint64 r) { m_Resolution = r; }
    igtlUint64 GetResolution()              { return m_Resolution; }

 protected:
    StartBindMessage();
    ~StartBindMessage() override = default;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    igtlUint64 m_Resolution;
};

class IGTLCommon_EXPORT StopBindMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::StopBindMessage, igtl::MessageBase);
    igtlNewMacro(igtl::StopBindMessage);

 protected:
    StopBindMessage()  { m_SendMessageType = "STP_BIND"; }
    ~StopBindMessage() override = default;
    igtlUint64 CalculateContentBufferSize() override { return 0; }
    int PackContent()   override { return 1; }
    int UnpackContent() override { return 1; }
};

class IGTLCommon_EXPORT RTSBindMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::RTSBindMessage, igtl::MessageBase);
    igtlNewMacro(igtl::RTSBindMessage);

    void      SetStatus(igtlUint8 s) { m_Status = s; }
    igtlUint8 GetStatus()             { return m_Status; }

 protected:
    RTSBindMessage();
    ~RTSBindMessage() override = default;
    igtlUint64 CalculateContentBufferSize() override { return 1; }
    int PackContent()   override;
    int UnpackContent() override;

    igtlUint8 m_Status;
};

}  // namespace igtl

#endif  // __igtlBindMessage_h
