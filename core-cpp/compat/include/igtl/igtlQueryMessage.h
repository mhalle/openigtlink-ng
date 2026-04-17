// Hand-written facade — supersedes the codegen stub.
//
// QUERY — generic data-type query, used as a base for various
// resource-discovery queries. 38-byte fixed header + variable-
// length device UID string:
//    4 B  queryID        (uint32)
//   32 B  queryDataType  (fixed ASCII, null-padded)
//    2 B  deviceUIDLength (uint16)
//    N B  deviceUID       (ASCII, exactly deviceUIDLength bytes)
#ifndef __igtlQueryMessage_h
#define __igtlQueryMessage_h

#include "igtlMacro.h"
#include "igtlMessageBase.h"
#include "igtlTypes.h"

#include <string>

#define IGTL_QUERY_DATE_TYPE_SIZE 32

namespace igtl {

class IGTLCommon_EXPORT QueryMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::QueryMessage, igtl::MessageBase);
    igtlNewMacro(igtl::QueryMessage);

    int         SetDeviceUID(const char* s);
    int         SetDeviceUID(const std::string& s);
    std::string GetDeviceUID();

    int         SetDataType(const char* s);
    int         SetDataType(const std::string& s);
    std::string GetDataType();

    void       SetQueryID(igtlUint32 id) { m_QueryID = id; }
    igtlUint32 GetQueryID()                { return m_QueryID; }

 protected:
    QueryMessage();
    ~QueryMessage() override = default;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    igtlUint32  m_QueryID;
    igtlUint8   m_DataType[IGTL_QUERY_DATE_TYPE_SIZE];
    std::string m_DeviceUID;
};

}  // namespace igtl

#endif  // __igtlQueryMessage_h
