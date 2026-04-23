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
// Also covers the sanctioned tier-2 protected API
// (GetContentPointer / GetContentSize / CopyReceivedFrom) that
// upstream-pattern subclasses target when compiled against us with
// OIGTL_NG_SHIM defined. See
// core-cpp/compat/API_COVERAGE.md §"Subclass extension API".

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

// -------- Tier-2 extension API: GetContentPointer + GetContentSize.
// The PLUS idiom is `(cast*)(this->m_Content + off)`; against us it
// becomes `(cast*)(this->GetContentPointer() + off)`. Verify the
// pointer is stable across PackContent (our test subclass writes a
// sentinel pattern) and that GetContentSize matches.
class ContentProbe : public igtl::MessageBase {
 public:
    igtlTypeMacro(ContentProbe, igtl::MessageBase);
    igtlNewMacro(ContentProbe);

    // Reads content bytes via the sanctioned accessor — mirrors PLUS's
    // pointer-arithmetic idiom transformed for the shim.
    std::uint8_t PeekByte(std::size_t off) const {
        return *(this->GetContentPointer() + off);
    }
    std::size_t ReportContentSize() const { return this->GetContentSize(); }

 protected:
    ContentProbe() { m_SendMessageType = "CONTENT_PROBE"; }
    ~ContentProbe() override = default;
    igtlUint64 CalculateContentBufferSize() override { return 16; }
    int PackContent() override {
        for (std::size_t i = 0; i < m_Content.size(); ++i) {
            m_Content[i] = static_cast<std::uint8_t>(0xA0 + i);
        }
        return 1;
    }
    int UnpackContent() override { return 1; }
};

void test_content_pointer_and_size() {
    auto p = ContentProbe::New();
    p->SetHeaderVersion(1);
    p->Pack();
    REQUIRE(p->ReportContentSize() == 16);
    // Sentinel pattern 0xA0, 0xA1, 0xA2, ... — every byte readable.
    for (std::size_t i = 0; i < 16; ++i) {
        REQUIRE(p->PeekByte(i) ==
                static_cast<std::uint8_t>(0xA0 + i));
    }
}

// Subclass of TransformMessage that exposes CopyReceivedFrom to
// the test harness. The method is protected on MessageBase (it's a
// subclass extension point, not public API); a subclass-side
// public wrapper is exactly how real consumers — PLUS's custom
// message classes in Shape-2 ifdef form — will access it.
class TransformCloneProbe : public igtl::TransformMessage {
 public:
    igtlTypeMacro(TransformCloneProbe, igtl::TransformMessage);
    igtlNewMacro(TransformCloneProbe);

    int TestCopyReceivedFrom(const igtl::MessageBase& other) {
        return this->CopyReceivedFrom(other);
    }

 protected:
    TransformCloneProbe() = default;
    ~TransformCloneProbe() override = default;
};

// -------- Tier-2 extension API: CopyReceivedFrom. Replaces upstream's
// InitBuffer/CopyHeader/AllocateBuffer/CopyBody sequence with one call.
void test_copy_received_from() {
    // Source: pack a TRANSFORM so we have real wire bytes + metadata.
    auto src = igtl::TransformMessage::New();
    src->SetHeaderVersion(2);
    src->SetDeviceName("Src");
    src->SetTimeStamp(1234u, 0u);
    src->SetMetaDataElement("StudyUID", IANA_TYPE_UTF_8, "1.2.3");
    igtl::Matrix4x4 m;
    igtl::IdentityMatrix(m);
    m[0][3] = 7.0f;
    src->SetMatrix(m);
    src->Pack();

    // Destination: probe subclass exposes CopyReceivedFrom.
    auto dst = TransformCloneProbe::New();
    dst->SetHeaderVersion(2);
    const int ok = dst->TestCopyReceivedFrom(*src);
    REQUIRE(ok == 1);

    // Destination now mirrors source: header fields, content,
    // metadata, wire image all match.
    REQUIRE(dst->GetDeviceName() == "Src");
    REQUIRE(dst->GetPackSize() == src->GetPackSize());
    std::string study;
    REQUIRE(dst->GetMetaDataElement("StudyUID", study));
    REQUIRE(study == "1.2.3");

    // Wire bytes are byte-identical — if CopyReceivedFrom truly
    // clones, `dst` can be sent on the network unchanged.
    REQUIRE(std::memcmp(dst->GetPackPointer(),
                        src->GetPackPointer(),
                        static_cast<std::size_t>(src->GetPackSize())) == 0);
}

// -------- CopyReceivedFrom refuses cross-version clones (stricter
// than upstream's permissive memcpy). Guards against accidentally
// producing a wrongly-framed message on the receive path.
void test_copy_received_from_rejects_version_mismatch() {
    auto src = igtl::TransformMessage::New();
    src->SetHeaderVersion(2);
    igtl::Matrix4x4 m; igtl::IdentityMatrix(m); src->SetMatrix(m);
    src->Pack();

    auto dst = TransformCloneProbe::New();
    dst->SetHeaderVersion(1);  // Deliberately different.
    REQUIRE(dst->TestCopyReceivedFrom(*src) == 0);
}

// -------- Feature-test macro OIGTL_NG_SHIM is visible to consumers.
// The macro lets PLUS-style forks branch on "am I linked against
// the hardened shim?" at compile time.
void test_feature_macro_defined() {
#ifndef OIGTL_NG_SHIM
    REQUIRE(false && "OIGTL_NG_SHIM should be defined by igtlMacro.h");
#endif
    // Sentinel body — compiling is the real check; this just forces
    // at-least-one-assertion accounting.
    REQUIRE(OIGTL_NG_SHIM == 1);
}

}  // namespace

int main() {
    test_initbuffer_sets_message_size();
    test_pack_updates_message_size();
    test_receive_path_message_size();
    test_content_pointer_and_size();
    test_copy_received_from();
    test_copy_received_from_rejects_version_mismatch();
    test_feature_macro_defined();

    if (g_fail_count > 0) {
        std::fprintf(stderr, "\nsubclass_protected_surface: %d FAIL(s)\n",
                     g_fail_count);
        return 1;
    }
    std::fprintf(stderr, "\nsubclass_protected_surface: all OK\n");
    return 0;
}
