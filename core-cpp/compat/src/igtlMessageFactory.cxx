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

namespace {
// Thunk: adapts `T::New()` (returning `SmartPointer<T>`) to the
// `PointerToMessageBaseNew` signature (returning
// `SmartPointer<MessageBase>`). Upstream uses C-style casts between
// the two function pointer types, which `-Wcast-function-type`
// (GCC ≥ 8) and `-Wcast-function-type-mismatch` (recent Clang)
// reject — and rightly so: calling one signature through the
// other is technically UB even if `SmartPointer<T>` layouts match.
// A stamped-out thunk per type is zero-cost at runtime and
// satisfies strict-warning builds on every platform we target.
template <class T>
igtl::MessageBase::Pointer NewAsBase() {
    typename T::Pointer p = T::New();
    return igtl::MessageBase::Pointer(p.GetPointer());
}
}  // namespace

// ---------------------------------------------------------------
// Construction — populates the built-in type map. Mirrors upstream's
// list one-to-one. Each entry goes through `NewAsBase<T>` above to
// get a properly-typed `MessageBase::Pointer (*)()` without a cast.
// ---------------------------------------------------------------
MessageFactory::MessageFactory() {
    // v1-era / always-available types.
    AddMessageType("TRANSFORM",
        &NewAsBase<igtl::TransformMessage>);
    AddMessageType("GET_TRANS",
        &NewAsBase<igtl::GetTransformMessage>);
    AddMessageType("POSITION",
        &NewAsBase<igtl::PositionMessage>);
    AddMessageType("IMAGE",
        &NewAsBase<igtl::ImageMessage>);
    AddMessageType("GET_IMAGE",
        &NewAsBase<igtl::GetImageMessage>);
    AddMessageType("STATUS",
        &NewAsBase<igtl::StatusMessage>);
    AddMessageType("GET_STATUS",
        &NewAsBase<igtl::GetStatusMessage>);
    AddMessageType("CAPABILITY",
        &NewAsBase<igtl::CapabilityMessage>);

    // v2 additions.
    AddMessageType("POINT",
        &NewAsBase<igtl::PointMessage>);
    AddMessageType("GET_POINT",
        &NewAsBase<igtl::GetPointMessage>);
    AddMessageType("TRAJ",
        &NewAsBase<igtl::TrajectoryMessage>);
    AddMessageType("GET_TRAJ",
        &NewAsBase<igtl::GetTrajectoryMessage>);
    AddMessageType("STRING",
        &NewAsBase<igtl::StringMessage>);
    AddMessageType("TDATA",
        &NewAsBase<igtl::TrackingDataMessage>);
    AddMessageType("POLYDATA",
        &NewAsBase<igtl::PolyDataMessage>);
    AddMessageType("GET_POLYDATA",
        &NewAsBase<igtl::GetPolyDataMessage>);
    AddMessageType("RTS_POLYDATA",
        &NewAsBase<igtl::RTSPolyDataMessage>);
    AddMessageType("STT_POLYDATA",
        &NewAsBase<igtl::StartPolyDataMessage>);
    AddMessageType("STP_POLYDATA",
        &NewAsBase<igtl::StopPolyDataMessage>);
    AddMessageType("RTS_TDATA",
        &NewAsBase<igtl::RTSTrackingDataMessage>);
    AddMessageType("STT_TDATA",
        &NewAsBase<igtl::StartTrackingDataMessage>);
    AddMessageType("STP_TDATA",
        &NewAsBase<igtl::StopTrackingDataMessage>);
    AddMessageType("QTDATA",
        &NewAsBase<igtl::QuaternionTrackingDataMessage>);
    AddMessageType("RTS_QTDATA",
        &NewAsBase<igtl::RTSQuaternionTrackingDataMessage>);
    AddMessageType("STT_QTDATA",
        &NewAsBase<igtl::StartQuaternionTrackingDataMessage>);
    AddMessageType("STP_QTDATA",
        &NewAsBase<igtl::StopQuaternionTrackingDataMessage>);
    AddMessageType("GET_IMGMETA",
        &NewAsBase<igtl::GetImageMetaMessage>);
    AddMessageType("IMGMETA",
        &NewAsBase<igtl::ImageMetaMessage>);
    AddMessageType("GET_LBMETA",
        &NewAsBase<igtl::GetLabelMetaMessage>);
    AddMessageType("LBMETA",
        &NewAsBase<igtl::LabelMetaMessage>);

    // v3 additions.
    AddMessageType("COMMAND",
        &NewAsBase<igtl::CommandMessage>);
    AddMessageType("RTS_COMMAND",
        &NewAsBase<igtl::RTSCommandMessage>);
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
