// Protected-member contract for upstream-pattern message subclasses.
//
// PLUS's custom message classes (PlusClientInfoMessage,
// PlusTrackedFrameMessage, PlusUsMessage) inherit from
// igtl::MessageBase / igtl::StringMessage / igtl::ImageMessage and
// reach into the base's *protected* members to compute body sizes
// and feed pack/unpack state. The most common idiom:
//
//   int bodySize = this->m_MessageSize - IGTL_HEADER_SIZE;
//
// …used inside Clone() and AllocateBuffer-then-copy plumbing.
//
// This test defines a minimal subclass that exercises that exact
// protected-member read and confirms `m_MessageSize` stays in sync
// with `m_Wire.size()` across every state transition a PLUS-style
// consumer would observe: InitBuffer → SetMessageHeader →
// AllocateBuffer → Pack. If this test compiles and passes, PLUS's
// `this->m_MessageSize - IGTL_HEADER_SIZE` idiom is safe against
// the shim for the parts of its receive path that live in the
// base class.
//
// NOTE — wider PLUS subclass-compile feasibility: beyond
// m_MessageSize, the three PLUS custom-message files also touch
// `m_Content` as a raw byte pointer (upstream has unsigned char*;
// our shim has std::vector<uint8_t>), `m_MetaDataMap`,
// `m_IsExtendedHeaderUnpacked`, and call `AllocateBuffer(bodySize)`
// (parameterised, whereas ours is nullary), plus `CopyHeader` /
// `CopyBody`. Closing those would broaden this test; see
// PLUS_AUDIT.md for the enumeration.

#include <cstdio>
#include <cstring>
#include <vector>

#include "igtl/igtl_header.h"
#include "igtl/igtlMessageBase.h"
#include "igtl/igtlMessageHeader.h"
#include "igtl/igtlStatusMessage.h"
#include "igtl/igtlTransformMessage.h"
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

// Subclass that mirrors PLUS's access pattern: read m_MessageSize
// from a protected context to derive a body size.
class ProtectedSurfaceProbe : public igtl::MessageBase {
 public:
    igtlTypeMacro(ProtectedSurfaceProbe, igtl::MessageBase);
    igtlNewMacro(ProtectedSurfaceProbe);

    // The PLUS idiom, spelled literally:
    igtlUint64 BodySizeFromMessageSize() const {
        // upstream / PLUS pattern: this->m_MessageSize - IGTL_HEADER_SIZE.
        return this->m_MessageSize - IGTL_HEADER_SIZE;
    }

    igtlUint64 ProbeMessageSize() const { return this->m_MessageSize; }

 protected:
    ProtectedSurfaceProbe() {
        m_SendMessageType = "PROBE";
    }
    ~ProtectedSurfaceProbe() override = default;

    igtlUint64 CalculateContentBufferSize() override {
        return 32;  // arbitrary body
    }
    int PackContent() override {
        // Write a simple sentinel pattern.
        for (std::size_t i = 0; i < m_Content.size(); ++i) {
            m_Content[i] = static_cast<std::uint8_t>(i & 0xFF);
        }
        return 1;
    }
    int UnpackContent() override { return 1; }
};

// -------- Test (1): InitBuffer seeds m_MessageSize to 58.
void test_initbuffer_sets_message_size() {
    auto p = ProtectedSurfaceProbe::New();
    p->InitBuffer();
    REQUIRE(p->ProbeMessageSize() ==
            static_cast<igtlUint64>(IGTL_HEADER_SIZE));
    REQUIRE(p->BodySizeFromMessageSize() == 0);
}

// -------- Test (2): After Pack, m_MessageSize == full wire size.
void test_pack_updates_message_size() {
    auto p = ProtectedSurfaceProbe::New();
    p->SetHeaderVersion(1);  // v1 keeps math simple: body == content
    p->Pack();
    REQUIRE(p->ProbeMessageSize() ==
            static_cast<igtlUint64>(p->GetPackSize()));
    REQUIRE(p->ProbeMessageSize() ==
            static_cast<igtlUint64>(IGTL_HEADER_SIZE + 32));
    REQUIRE(p->BodySizeFromMessageSize() == 32);
}

// -------- Test (3): SetMessageHeader → AllocateBuffer mirrors the
// receive-path sequence. m_MessageSize should end at 58+bodySize.
void test_receive_path_message_size() {
    // Sender — build a real TRANSFORM so we have a valid 58-byte
    // header to hand to the subclass probe through a MessageHeader.
    auto tx = igtl::TransformMessage::New();
    tx->SetHeaderVersion(2);
    tx->SetDeviceName("Probe");
    tx->SetTimeStamp(1u, 0u);
    igtl::Matrix4x4 m;
    igtl::IdentityMatrix(m);
    tx->SetMatrix(m);
    tx->Pack();

    const std::size_t packed_size =
        static_cast<std::size_t>(tx->GetPackSize());
    std::vector<std::uint8_t> packed(packed_size);
    std::memcpy(packed.data(), tx->GetPackPointer(), packed_size);

    // MessageHeader step — same as upstream / PLUS.
    auto hdr = igtl::MessageHeader::New();
    hdr->InitBuffer();
    std::memcpy(hdr->GetPackPointer(), packed.data(), IGTL_HEADER_SIZE);
    hdr->Unpack();

    // Subclass probe — take the header, allocate the body.
    auto p = ProtectedSurfaceProbe::New();
    p->SetMessageHeader(hdr);
    const igtlUint64 seeded = p->ProbeMessageSize();
    REQUIRE(seeded ==
        static_cast<igtlUint64>(IGTL_HEADER_SIZE + hdr->GetBodySizeToRead()));

    p->AllocateBuffer();
    REQUIRE(p->ProbeMessageSize() ==
        static_cast<igtlUint64>(IGTL_HEADER_SIZE + hdr->GetBodySizeToRead()));
    REQUIRE(p->BodySizeFromMessageSize() == hdr->GetBodySizeToRead());
}

}  // namespace

int main() {
    test_initbuffer_sets_message_size();
    test_pack_updates_message_size();
    test_receive_path_message_size();

    if (g_fail_count > 0) {
        std::fprintf(stderr, "\nsubclass_protected_surface: %d FAIL(s)\n",
                     g_fail_count);
        return 1;
    }
    std::fprintf(stderr, "\nsubclass_protected_surface: all OK\n");
    return 0;
}
