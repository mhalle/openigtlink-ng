// Hand-written facade — supersedes the codegen stub.
//
// CAPABILITY — array of 12-byte null-padded type-id strings.
// Total body = 12 × N bytes.
#ifndef __igtlCapabilityMessage_h
#define __igtlCapabilityMessage_h

#include "igtlMacro.h"
#include "igtlMessageBase.h"
#include "igtlTypes.h"

#include <string>
#include <vector>

namespace igtl {

class IGTLCommon_EXPORT CapabilityMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::CapabilityMessage, igtl::MessageBase);
    igtlNewMacro(igtl::CapabilityMessage);

    void                     SetTypes(std::vector<std::string> types);
    int                      SetType(int id, const char* name);
    const char*              GetType(int id);
    void                     SetNumberOfTypes(int n) {
        m_TypeNames.resize(static_cast<std::size_t>(n));
    }
    int                      GetNumberOfTypes() {
        return static_cast<int>(m_TypeNames.size());
    }
    std::vector<std::string> GetTypes() { return m_TypeNames; }

 protected:
    CapabilityMessage();
    ~CapabilityMessage() override = default;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    std::vector<std::string> m_TypeNames;
};

}  // namespace igtl

#endif  // __igtlCapabilityMessage_h
