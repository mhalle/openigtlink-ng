// Hand-written facade — supersedes the codegen stub.
//
// COMMAND — remote command dispatch.
// Wire layout: 138-byte fixed header + `length` bytes of command text.
//   4 B  command_id    (uint32)
//   128 B command_name (fixed_string)
//   2 B  encoding      (uint16, IANA MIBenum)
//   4 B  length        (uint32, bytes of command text)
//   N B  command       (raw bytes, `length` long)
#ifndef __igtlCommandMessage_h
#define __igtlCommandMessage_h

#include "igtlMacro.h"
#include "igtlMessageBase.h"
#include "igtlTypes.h"

#include <string>

#define IGTL_COMMAND_NAME_SIZE 128

namespace igtl {

class IGTLCommon_EXPORT CommandMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::CommandMessage, igtl::MessageBase);
    igtlNewMacro(igtl::CommandMessage);

    int         SetCommandId(igtlUint32 id);
    igtlUint32  GetCommandId() const { return m_CommandId; }

    int         SetCommandName(const char* n);
    int         SetCommandName(const std::string& n);
    std::string GetCommandName() const;

    int         SetCommandContent(const char* s);
    int         SetCommandContent(const std::string& s);
    std::string GetCommandContent() const { return m_Command; }
    igtlUint32  GetCommandContentLength() const {
        return static_cast<igtlUint32>(m_Command.size());
    }

    int         SetContentEncoding(igtlUint16 e);
    igtlUint16  GetContentEncoding() const { return m_Encoding; }

 protected:
    CommandMessage();
    ~CommandMessage() override = default;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    igtlUint32  m_CommandId;
    igtlUint8   m_CommandName[IGTL_COMMAND_NAME_SIZE];
    igtlUint16  m_Encoding;
    std::string m_Command;
};

class IGTLCommon_EXPORT RTSCommandMessage : public CommandMessage {
 public:
    igtlTypeMacro(igtl::RTSCommandMessage, igtl::CommandMessage);
    igtlNewMacro(igtl::RTSCommandMessage);

    int         SetCommandErrorString(const char* s);
    int         SetCommandErrorString(const std::string& s);
    std::string GetCommandErrorString() const;

 protected:
    RTSCommandMessage();
    ~RTSCommandMessage() override = default;
};

}  // namespace igtl

#endif  // __igtlCommandMessage_h
