// Shim sanity — igtl::MessageFactory mirrors upstream's surface.
//
// Exercises the exact pattern PLUS uses in vtkPlusOpenIGTLinkServer
// and related consumers:
//   1. CreateHeaderMessage(2) → MessageHeader, InitPack-ready.
//   2. CreateSendMessage("TRANSFORM", 2) → TransformMessage, typed.
//   3. Pack that TRANSFORM, slice out the 58-byte header region,
//      replay it through CreateReceiveMessage → matching typed
//      receiver. Confirms AllocateBuffer() sized m_Body correctly.
//   4. AddMessageType() with a user-registered extension class →
//      subsequent CreateSendMessage mints that exact class.
//   5. GetAvailableMessageTypes returns the full built-in list plus
//      any extensions.
//
// No GoogleTest — same REQUIRE idiom as the sibling compat tests.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

#include "igtl/igtlMessageFactory.h"
#include "igtl/igtlMessageHeader.h"
#include "igtl/igtlTransformMessage.h"
#include "igtl/igtlStatusMessage.h"
#include "igtl/igtlMath.h"

namespace {

int g_fail_count = 0;

#define REQUIRE(expr) do {                                            \
    if (!(expr)) {                                                    \
        std::fprintf(stderr, "  FAIL: %s:%d  %s\n",                   \
                     __FILE__, __LINE__, #expr);                      \
        ++g_fail_count;                                               \
    }                                                                 \
} while (0)

// --------- Test (1): CreateHeaderMessage returns a usable HEADER.
void test_create_header_message() {
    auto fac = igtl::MessageFactory::New();
    auto hdr = fac->CreateHeaderMessage(2);
    REQUIRE(hdr.IsNotNull());
    REQUIRE(hdr->GetHeaderVersion() == 2);
}

// --------- Test (2): CreateSendMessage mints built-in types.
void test_create_send_message_builtin() {
    auto fac = igtl::MessageFactory::New();
    auto m = fac->CreateSendMessage("TRANSFORM", 2);
    REQUIRE(m.IsNotNull());
    REQUIRE(std::string(m->GetDeviceType()) == "TRANSFORM");

    auto s = fac->CreateSendMessage("STATUS", 2);
    REQUIRE(s.IsNotNull());
    REQUIRE(std::string(s->GetDeviceType()) == "STATUS");

    // Unknown type returns null.
    auto unknown = fac->CreateSendMessage("NOT_A_TYPE", 2);
    REQUIRE(unknown.IsNull());

    // Case-insensitive lookup (upstream behavior).
    auto lower = fac->CreateSendMessage("transform", 2);
    REQUIRE(lower.IsNotNull());
}

// --------- Test (3): Receive-path round-trip.
// Pack a TRANSFORM, extract its 58-byte header, drive it through
// MessageHeader::Unpack, then CreateReceiveMessage, then
// Unpack body. Confirms the factory sizes the buffer correctly.
void test_receive_roundtrip() {
    // Sender
    auto tx = igtl::TransformMessage::New();
    tx->SetHeaderVersion(2);
    tx->SetDeviceName("Tracker_01");
    tx->SetTimeStamp(1718455896u, 0u);
    igtl::Matrix4x4 m;
    igtl::IdentityMatrix(m);
    m[0][3] = 11.0f;
    m[1][3] = 22.0f;
    m[2][3] = 33.0f;
    tx->SetMatrix(m);
    tx->Pack();

    const std::size_t packed_size =
        static_cast<std::size_t>(tx->GetPackSize());
    REQUIRE(packed_size >= 58);
    std::vector<std::uint8_t> packed(packed_size);
    std::memcpy(packed.data(), tx->GetPackPointer(), packed_size);

    // Receiver — step (a): consume fixed 58-byte header.
    auto fac = igtl::MessageFactory::New();
    auto hdr = fac->CreateHeaderMessage(2);
    REQUIRE(hdr.IsNotNull());
    REQUIRE(hdr->GetPackSize() == 58u);
    std::memcpy(hdr->GetPackPointer(), packed.data(), 58);
    hdr->Unpack();
    REQUIRE(std::string(hdr->GetDeviceType()) == "TRANSFORM");
    REQUIRE(fac->IsValid(hdr));

    // step (b): factory mints the typed message with buffer sized.
    auto msg = fac->CreateReceiveMessage(hdr);
    REQUIRE(msg.IsNotNull());
    REQUIRE(std::string(msg->GetDeviceType()) == "TRANSFORM");
    const std::size_t body_size =
        static_cast<std::size_t>(msg->GetPackBodySize());
    REQUIRE(body_size == packed_size - 58);

    // step (c): deposit body bytes, Unpack, verify matrix.
    std::memcpy(msg->GetPackBodyPointer(),
                packed.data() + 58, body_size);
    msg->Unpack();

    auto* typed = dynamic_cast<igtl::TransformMessage*>(
        msg.GetPointer());
    REQUIRE(typed != nullptr);
    if (typed != nullptr) {
        igtl::Matrix4x4 got;
        typed->GetMatrix(got);
        REQUIRE(got[0][3] == 11.0f);
        REQUIRE(got[1][3] == 22.0f);
        REQUIRE(got[2][3] == 33.0f);
    }
}

// --------- Test (4): AddMessageType — registering a user extension
// and confirming CreateSendMessage mints it. Uses TransformMessage
// masquerading as a different type-id so we don't need a new class —
// the factory just stores a `New()` pointer and a name.
void test_add_message_type_extension() {
    auto fac = igtl::MessageFactory::New();
    fac->AddMessageType("CUSTOM_TYPE",
        (igtl::MessageFactory::PointerToMessageBaseNew)
            &igtl::TransformMessage::New);

    auto m = fac->CreateSendMessage("CUSTOM_TYPE", 2);
    REQUIRE(m.IsNotNull());

    REQUIRE(fac->GetMessageTypeNewPointer("CUSTOM_TYPE") != nullptr);
    REQUIRE(fac->GetMessageTypeNewPointer("NOPE") == nullptr);
}

// --------- Test (5): GetAvailableMessageTypes contains the built-ins
// PLUS users expect to find.
void test_get_available_message_types() {
    auto fac = igtl::MessageFactory::New();
    std::vector<std::string> types;
    fac->GetAvailableMessageTypes(types);
    REQUIRE(!types.empty());

    auto has = [&](const char* want) {
        return std::find(types.begin(), types.end(), std::string(want))
               != types.end();
    };
    // A representative slice — the types PLUS depends on most.
    REQUIRE(has("TRANSFORM"));
    REQUIRE(has("STATUS"));
    REQUIRE(has("STRING"));
    REQUIRE(has("TDATA"));
    REQUIRE(has("STT_TDATA"));
    REQUIRE(has("STP_TDATA"));
    REQUIRE(has("RTS_TDATA"));
    REQUIRE(has("COMMAND"));
    REQUIRE(has("RTS_COMMAND"));
    REQUIRE(has("POLYDATA"));
    REQUIRE(has("IMAGE"));
}

}  // namespace

int main() {
    test_create_header_message();
    test_create_send_message_builtin();
    test_receive_roundtrip();
    test_add_message_type_extension();
    test_get_available_message_types();

    if (g_fail_count > 0) {
        std::fprintf(stderr, "\nmessage_factory: %d FAIL(s)\n",
                     g_fail_count);
        return 1;
    }
    std::fprintf(stderr, "\nmessage_factory: all OK\n");
    return 0;
}
