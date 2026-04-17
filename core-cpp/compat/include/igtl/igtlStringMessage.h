// Hand-written facade — supersedes the codegen stub.
//
// STRING — character string with an explicit encoding hint.
// Wire layout (per spec/schemas/string.json):
//   2 B  uint16 encoding     — IANA MIBenum (3 = US-ASCII, 106 = UTF-8)
//   2 B  uint16 length       — byte length of the following payload
//   N B  payload             — `length` bytes, no terminator
// Total body: 4 + length bytes.
//
// Upstream quirk (noted in the schema): unpack is not bounds-checked.
// Our shim's UnpackContent clamps length to the available content
// region; sending a too-large length safely returns failure rather
// than OOB-reading like upstream does.
#ifndef __igtlStringMessage_h
#define __igtlStringMessage_h

#include "igtlMacro.h"
#include "igtlMessageBase.h"
#include "igtlTypes.h"

#include <string>

namespace igtl {

class IGTLCommon_EXPORT StringMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::StringMessage, igtl::MessageBase);
    igtlNewMacro(igtl::StringMessage);

    int SetString(const char* s);
    int SetString(const std::string& s);

    int SetEncoding(igtlUint16 enc);

    const char* GetString();
    igtlUint16  GetEncoding();

 protected:
    StringMessage();
    ~StringMessage() override;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    igtlUint16  m_Encoding;
    std::string m_String;
};

}  // namespace igtl

#endif  // __igtlStringMessage_h
