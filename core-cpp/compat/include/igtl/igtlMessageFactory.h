// igtlMessageFactory.h — shim facade matching upstream's
// `igtl::MessageFactory` drop-in.
//
// Role: on the receive path, consumers read the fixed 58-byte header
// into a MessageHeader, Unpack() it, then ask the factory to produce
// a typed MessageBase ready to take the body:
//
//     igtl::MessageFactory::Pointer fac = igtl::MessageFactory::New();
//     auto hdr = igtl::MessageHeader::New();
//     hdr->InitPack();
//     sock->Receive(hdr->GetPackPointer(), hdr->GetPackSize());
//     hdr->Unpack();
//     igtl::MessageBase::Pointer msg = fac->CreateReceiveMessage(hdr);
//     sock->Receive(msg->GetPackBodyPointer(), msg->GetPackBodySize());
//     msg->Unpack();
//
// Consumers (PLUS, Slicer) also call `AddMessageType` to register
// their own extension classes, and `CreateSendMessage` to mint empty
// ones by type-id. The full surface — `AddMessageType`,
// `GetMessageTypeNewPointer`, `CreateHeaderMessage`,
// `CreateSendMessage`, `CreateReceiveMessage`, `GetMessage`,
// `IsValid`, `GetAvailableMessageTypes` — is mirrored one-for-one.
//
// Implementation is purely a `std::map<std::string,
// PointerToMessageBaseNew>` populated in the constructor with every
// built-in message type the shim exposes. No bridge to core-cpp's
// runtime registry is needed: the factory is self-contained, as in
// upstream.
#ifndef __igtlMessageFactory_h
#define __igtlMessageFactory_h

#include "igtlMacro.h"
#include "igtlMessageBase.h"
#include "igtlMessageHeader.h"
#include "igtlObject.h"

#include <map>
#include <string>
#include <vector>

namespace igtl {

class IGTLCommon_EXPORT MessageFactory : public Object {
 public:
    igtlTypeMacro(igtl::MessageFactory, igtl::Object);
    igtlNewMacro(igtl::MessageFactory);

    // Function pointer to a `static Pointer New()` method of any
    // `MessageBase` subclass. Upstream typedefs this to return
    // `MessageBase::Pointer`; consumers use a C-style cast from
    // `FooMessage::Pointer (*)()` to this signature. Layout-wise
    // every `SmartPointer<T>` is the same size, which is why the
    // cast works despite being technically UB.
    typedef igtl::MessageBase::Pointer (*PointerToMessageBaseNew)();

    virtual void AddMessageType(const std::string& messageTypeName,
                                PointerToMessageBaseNew messageTypeNewPointer);

    virtual PointerToMessageBaseNew GetMessageTypeNewPointer(
        const std::string& messageTypeName) const;

    bool IsValid(igtl::MessageHeader::Pointer headerMsg);
    bool IsValid(igtl::MessageHeader::Pointer headerMsg) const;

    // LEGACY method, use CreateReceiveMessage instead.
    igtl::MessageBase::Pointer GetMessage(
        igtl::MessageHeader::Pointer headerMsg);

    igtl::MessageHeader::Pointer CreateHeaderMessage(int headerVersion) const;

    igtl::MessageBase::Pointer CreateReceiveMessage(
        igtl::MessageHeader::Pointer headerMsg) const;

    igtl::MessageBase::Pointer CreateSendMessage(
        const std::string& messageType, int headerVersion) const;

    void GetAvailableMessageTypes(std::vector<std::string>& types) const;

 protected:
    MessageFactory();
    ~MessageFactory() override = default;

 private:
    std::map<std::string, PointerToMessageBaseNew> IgtlMessageTypes;
};

}  // namespace igtl

#endif  // __igtlMessageFactory_h
