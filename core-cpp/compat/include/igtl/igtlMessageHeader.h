// igtlMessageHeader.h — header-only message (no body unpacking).
//
// Upstream uses MessageHeader on the receive path:
//
//   auto hdr = MessageHeader::New();
//   hdr->InitPack();
//   sock->Receive(hdr->GetPackPointer(), hdr->GetPackSize());
//   hdr->Unpack();
//   std::size_t body_size = hdr->GetBodySizeToRead();
//   // dispatch by hdr->GetDeviceType() / hdr->GetMessageType()
//
// Downstream then allocates a concrete message, copies the header
// in via `msg->SetMessageHeader(hdr)`, and reads the body.
#ifndef __igtlMessageHeader_h
#define __igtlMessageHeader_h

#include "igtlMessageBase.h"

namespace igtl {

class IGTLCommon_EXPORT MessageHeader : public MessageBase {
 public:
    igtlTypeMacro(igtl::MessageHeader, igtl::MessageBase);
    igtlNewMacro(igtl::MessageHeader);

 protected:
    MessageHeader() {
        m_SendMessageType = "HEADER";
    }
    ~MessageHeader() override = default;

    // Header-only: no content to serialise.
    int PackContent()   override { return 0; }
    int UnpackContent() override { return 0; }
    igtlUint64 CalculateContentBufferSize() override { return 0; }
};

// `HeaderOnlyMessageBase` — the upstream name used by GET_/STT_/STP_/
// RTS_ messages that carry no body of their own.
using HeaderOnlyMessageBase = MessageHeader;

}  // namespace igtl

#endif  // __igtlMessageHeader_h
