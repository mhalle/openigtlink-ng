// igtlMessageFactory.cxx — implementation. Direct port of upstream's
// `Source/igtlMessageFactory.cxx`; the constructor is the registration
// list, the rest is plain map lookups.

#include "igtl/igtlMessageFactory.h"

// Pull in every shim-exposed message type so their `New()` function
// pointers are addressable.
#include "igtl/igtlCapabilityMessage.h"
#include "igtl/igtlClientSocket.h"
#include "igtl/igtlCommandMessage.h"
#include "igtl/igtlGetImageMessage.h"
#include "igtl/igtlGetImageMetaMessage.h"
#include "igtl/igtlGetLabelMetaMessage.h"
#include "igtl/igtlGetPointMessage.h"
#include "igtl/igtlGetPolyDataMessage.h"
#include "igtl/igtlGetStatusMessage.h"
#include "igtl/igtlGetTrajectoryMessage.h"
#include "igtl/igtlImageMessage.h"
#include "igtl/igtlImageMetaMessage.h"
#include "igtl/igtlLabelMetaMessage.h"
#include "igtl/igtlPointMessage.h"
#include "igtl/igtlPolyDataMessage.h"
#include "igtl/igtlPositionMessage.h"
#include "igtl/igtlQuaternionTrackingDataMessage.h"
#include "igtl/igtlRTSQuaternionTrackingDataMessage.h"
#include "igtl/igtlRTSTrackingDataMessage.h"
#include "igtl/igtlStartPolyDataMessage.h"
#include "igtl/igtlStartQuaternionTrackingDataMessage.h"
#include "igtl/igtlStartTrackingDataMessage.h"
#include "igtl/igtlStatusMessage.h"
#include "igtl/igtlStopPolyDataMessage.h"
#include "igtl/igtlStopQuaternionTrackingDataMessage.h"
#include "igtl/igtlStopTrackingDataMessage.h"
#include "igtl/igtlStringMessage.h"
#include "igtl/igtlTrackingDataMessage.h"
#include "igtl/igtlTrajectoryMessage.h"
#include "igtl/igtlTransformMessage.h"

#include <algorithm>
#include <cassert>
#include <cctype>

namespace igtl {

// ---------------------------------------------------------------
// Construction — populates the built-in type map. Mirrors upstream's
// list one-to-one. The C-style casts from `FooMessage::Pointer (*)()`
// to `MessageBase::Pointer (*)()` are upstream's chosen form; they
// compile because every `SmartPointer<T>` has the same layout.
// ---------------------------------------------------------------
MessageFactory::MessageFactory() {
    // v1-era / always-available types.
    AddMessageType("TRANSFORM",
        (PointerToMessageBaseNew)&igtl::TransformMessage::New);
    AddMessageType("GET_TRANS",
        (PointerToMessageBaseNew)&igtl::GetTransformMessage::New);
    AddMessageType("POSITION",
        (PointerToMessageBaseNew)&igtl::PositionMessage::New);
    AddMessageType("IMAGE",
        (PointerToMessageBaseNew)&igtl::ImageMessage::New);
    AddMessageType("GET_IMAGE",
        (PointerToMessageBaseNew)&igtl::GetImageMessage::New);
    AddMessageType("STATUS",
        (PointerToMessageBaseNew)&igtl::StatusMessage::New);
    AddMessageType("GET_STATUS",
        (PointerToMessageBaseNew)&igtl::GetStatusMessage::New);
    AddMessageType("CAPABILITY",
        (PointerToMessageBaseNew)&igtl::CapabilityMessage::New);

    // v2 additions.
    AddMessageType("POINT",
        (PointerToMessageBaseNew)&igtl::PointMessage::New);
    AddMessageType("GET_POINT",
        (PointerToMessageBaseNew)&igtl::GetPointMessage::New);
    AddMessageType("TRAJ",
        (PointerToMessageBaseNew)&igtl::TrajectoryMessage::New);
    AddMessageType("GET_TRAJ",
        (PointerToMessageBaseNew)&igtl::GetTrajectoryMessage::New);
    AddMessageType("STRING",
        (PointerToMessageBaseNew)&igtl::StringMessage::New);
    AddMessageType("TDATA",
        (PointerToMessageBaseNew)&igtl::TrackingDataMessage::New);
    AddMessageType("POLYDATA",
        (PointerToMessageBaseNew)&igtl::PolyDataMessage::New);
    AddMessageType("GET_POLYDATA",
        (PointerToMessageBaseNew)&igtl::GetPolyDataMessage::New);
    AddMessageType("RTS_POLYDATA",
        (PointerToMessageBaseNew)&igtl::RTSPolyDataMessage::New);
    AddMessageType("STT_POLYDATA",
        (PointerToMessageBaseNew)&igtl::StartPolyDataMessage::New);
    AddMessageType("STP_POLYDATA",
        (PointerToMessageBaseNew)&igtl::StopPolyDataMessage::New);
    AddMessageType("RTS_TDATA",
        (PointerToMessageBaseNew)&igtl::RTSTrackingDataMessage::New);
    AddMessageType("STT_TDATA",
        (PointerToMessageBaseNew)&igtl::StartTrackingDataMessage::New);
    AddMessageType("STP_TDATA",
        (PointerToMessageBaseNew)&igtl::StopTrackingDataMessage::New);
    AddMessageType("QTDATA",
        (PointerToMessageBaseNew)&igtl::QuaternionTrackingDataMessage::New);
    AddMessageType("RTS_QTDATA",
        (PointerToMessageBaseNew)&igtl::RTSQuaternionTrackingDataMessage::New);
    AddMessageType("STT_QTDATA",
        (PointerToMessageBaseNew)&igtl::StartQuaternionTrackingDataMessage::New);
    AddMessageType("STP_QTDATA",
        (PointerToMessageBaseNew)&igtl::StopQuaternionTrackingDataMessage::New);
    AddMessageType("GET_IMGMETA",
        (PointerToMessageBaseNew)&igtl::GetImageMetaMessage::New);
    AddMessageType("IMGMETA",
        (PointerToMessageBaseNew)&igtl::ImageMetaMessage::New);
    AddMessageType("GET_LBMETA",
        (PointerToMessageBaseNew)&igtl::GetLabelMetaMessage::New);
    AddMessageType("LBMETA",
        (PointerToMessageBaseNew)&igtl::LabelMetaMessage::New);

    // v3 additions.
    AddMessageType("COMMAND",
        (PointerToMessageBaseNew)&igtl::CommandMessage::New);
    AddMessageType("RTS_COMMAND",
        (PointerToMessageBaseNew)&igtl::RTSCommandMessage::New);
}

// ---------------------------------------------------------------
// Map mutators / lookup.
// ---------------------------------------------------------------
void MessageFactory::AddMessageType(
    const std::string& messageTypeName,
    MessageFactory::PointerToMessageBaseNew messageTypeNewPointer) {
    IgtlMessageTypes[messageTypeName] = messageTypeNewPointer;
}

MessageFactory::PointerToMessageBaseNew
MessageFactory::GetMessageTypeNewPointer(
    const std::string& messageTypeName) const {
    auto it = IgtlMessageTypes.find(messageTypeName);
    if (it == IgtlMessageTypes.end()) return nullptr;
    return it->second;
}

// ---------------------------------------------------------------
// IsValid — non-const and const overloads, same body.
// ---------------------------------------------------------------
bool MessageFactory::IsValid(igtl::MessageHeader::Pointer headerMsg) {
    if (headerMsg.IsNull()) return false;
    const std::string t = headerMsg->GetMessageType();
    return IgtlMessageTypes.find(t) != IgtlMessageTypes.end();
}

bool MessageFactory::IsValid(igtl::MessageHeader::Pointer headerMsg) const {
    if (headerMsg.IsNull()) return false;
    const std::string t = headerMsg->GetMessageType();
    return IgtlMessageTypes.find(t) != IgtlMessageTypes.end();
}

// ---------------------------------------------------------------
// GetMessage — legacy path. Creates the typed message, deposits the
// received header, then calls InitBuffer (equivalent to upstream's
// AllocateBuffer-after-SetMessageHeader idiom).
// ---------------------------------------------------------------
igtl::MessageBase::Pointer MessageFactory::GetMessage(
    igtl::MessageHeader::Pointer headerMsg) {
    if (headerMsg.IsNull()) return nullptr;
    if (!IsValid(headerMsg)) return nullptr;

    std::string t = headerMsg->GetMessageType();
    std::transform(t.begin(), t.end(), t.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::toupper(c));
                   });

    PointerToMessageBaseNew ctor = GetMessageTypeNewPointer(t);
    if (ctor == nullptr) return nullptr;
    igtl::MessageBase::Pointer result = ctor();
    assert(result.IsNotNull());

    result->SetMessageHeader(headerMsg);
    result->InitBuffer();
    return result;
}

igtl::MessageHeader::Pointer MessageFactory::CreateHeaderMessage(
    int headerVersion) const {
    igtl::MessageHeader::Pointer hdr = igtl::MessageHeader::New();
    hdr->SetHeaderVersion(static_cast<unsigned short>(headerVersion));
    hdr->InitBuffer();
    return hdr;
}

igtl::MessageBase::Pointer MessageFactory::CreateReceiveMessage(
    igtl::MessageHeader::Pointer headerMsg) const {
    if (headerMsg.IsNull()) return nullptr;
    if (!IsValid(headerMsg)) return nullptr;

    std::string t = headerMsg->GetMessageType();
    std::transform(t.begin(), t.end(), t.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::toupper(c));
                   });

    PointerToMessageBaseNew ctor = GetMessageTypeNewPointer(t);
    if (ctor == nullptr) return nullptr;
    igtl::MessageBase::Pointer result = ctor();
    assert(result.IsNotNull());

    result->SetMessageHeader(headerMsg);
    result->AllocateBuffer();
    return result;
}

igtl::MessageBase::Pointer MessageFactory::CreateSendMessage(
    const std::string& messageType, int headerVersion) const {
    if (messageType.empty()) return nullptr;

    std::string t = messageType;
    std::transform(t.begin(), t.end(), t.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::toupper(c));
                   });

    PointerToMessageBaseNew ctor = GetMessageTypeNewPointer(t);
    if (ctor == nullptr) return nullptr;
    igtl::MessageBase::Pointer result = ctor();
    assert(result.IsNotNull());

    result->SetDeviceType(t);
    result->SetHeaderVersion(static_cast<unsigned short>(headerVersion));
    result->InitBuffer();
    return result;
}

void MessageFactory::GetAvailableMessageTypes(
    std::vector<std::string>& types) const {
    types.clear();
    types.reserve(IgtlMessageTypes.size());
    for (const auto& kv : IgtlMessageTypes) {
        types.push_back(kv.first);
    }
}

}  // namespace igtl
