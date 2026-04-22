// Shim audit — v3 metadata + header-version API surface.
//
// PLUS and related v3 consumers drive metadata through three entry
// points on MessageBase:
//
//   SetMetaDataElement(key, enc, value)   — writer side
//   GetMetaDataElement(key, [enc,] value) — reader side (two overloads)
//   GetMetaData()                         — whole-map accessor
//
// …plus GetHeaderVersion() / SetHeaderVersion() for negotiating the
// framing (v1 = bare body; v2/v3 = 12-byte ext-header + metadata
// region). A round-trip here proves the shim's Pack/Unpack preserves
// metadata byte-for-byte and reports the correct header version on
// the receive side, which is the contract every v3 consumer depends
// on.
//
// This file intentionally tests ONLY the string-valued SetMetaDataElement
// overload that upstream's igtlMessageBase.h also has. Upstream
// additionally offers typed-scalar overloads (igtl_uint8, igtl_int8,
// …, double). If Phase 3's compat_plus_interop build surfaces a
// dependency on those overloads, add them then; for now the typed
// overloads are out-of-scope because every documented PLUS metadata
// write path uses strings.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "igtl/igtlMessageFactory.h"
#include "igtl/igtlMessageHeader.h"
#include "igtl/igtlStringMessage.h"
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

// -------- Test (1): v2 metadata round-trip via a TRANSFORM.
// Write two metadata entries on send, Pack, route through
// MessageFactory on the receive side, Unpack, read both back out.
// Covers the common PLUS pattern for attaching PatientID / StudyUID
// alongside a tracking transform.
void test_metadata_roundtrip_v2() {
    auto tx = igtl::TransformMessage::New();
    tx->SetHeaderVersion(2);
    tx->SetDeviceName("Tracker_01");
    tx->SetTimeStamp(1718455896u, 0u);

    igtl::Matrix4x4 m;
    igtl::IdentityMatrix(m);
    tx->SetMatrix(m);

    REQUIRE(tx->SetMetaDataElement("PatientID",
        IANA_TYPE_US_ASCII, "P-001"));
    REQUIRE(tx->SetMetaDataElement("StudyUID",
        IANA_TYPE_UTF_8, "1.2.826.0.1.3680043"));
    tx->Pack();

    const std::size_t packed_size =
        static_cast<std::size_t>(tx->GetPackSize());
    std::vector<std::uint8_t> packed(packed_size);
    std::memcpy(packed.data(), tx->GetPackPointer(), packed_size);

    // Receiver path via MessageFactory, mirroring PLUS.
    auto fac = igtl::MessageFactory::New();
    auto hdr = fac->CreateHeaderMessage(2);
    std::memcpy(hdr->GetPackPointer(), packed.data(), 58);
    hdr->Unpack();
    REQUIRE(fac->IsValid(hdr));
    REQUIRE(hdr->GetHeaderVersion() == 2);

    auto msg = fac->CreateReceiveMessage(hdr);
    REQUIRE(msg.IsNotNull());
    const std::size_t body_size =
        static_cast<std::size_t>(msg->GetPackBodySize());
    std::memcpy(msg->GetPackBodyPointer(),
                packed.data() + 58, body_size);
    msg->Unpack();

    REQUIRE(msg->GetHeaderVersion() == 2);

    // Two-argument form (value only).
    std::string v;
    REQUIRE(msg->GetMetaDataElement("PatientID", v));
    REQUIRE(v == "P-001");

    // Three-argument form (value + encoding round-tripped).
    IANA_ENCODING_TYPE enc = IANA_TYPE_US_ASCII;
    std::string v2;
    REQUIRE(msg->GetMetaDataElement("StudyUID", enc, v2));
    REQUIRE(v2 == "1.2.826.0.1.3680043");
    REQUIRE(enc == IANA_TYPE_UTF_8);

    // Missing key returns false, value untouched.
    std::string missing = "SENTINEL";
    REQUIRE(!msg->GetMetaDataElement("NotPresent", missing));
    REQUIRE(missing == "SENTINEL");

    // Whole-map accessor.
    const auto& mp = msg->GetMetaData();
    REQUIRE(mp.size() == 2);
    REQUIRE(mp.find("PatientID") != mp.end());
    REQUIRE(mp.find("StudyUID") != mp.end());
}

// -------- Test (2): accept SetHeaderVersion(3) and emit v2-compatible
// bytes (our contract: v3 framing == v2 framing for in-scope types).
// PLUS sends v3-declared messages; receivers must still decode.
void test_header_version_3_accepted() {
    auto s = igtl::StringMessage::New();
    s->SetHeaderVersion(3);
    s->SetDeviceName("Dev");
    s->SetString("hello");
    s->SetMetaDataElement("role", IANA_TYPE_US_ASCII, "probe");
    s->Pack();

    // Round-trip through the factory: Pack with v=3, receiver parses
    // the header back, GetHeaderVersion reflects what we sent.
    const std::size_t packed_size =
        static_cast<std::size_t>(s->GetPackSize());
    std::vector<std::uint8_t> packed(packed_size);
    std::memcpy(packed.data(), s->GetPackPointer(), packed_size);

    auto fac = igtl::MessageFactory::New();
    auto hdr = fac->CreateHeaderMessage(3);
    std::memcpy(hdr->GetPackPointer(), packed.data(), 58);
    hdr->Unpack();
    REQUIRE(hdr->GetHeaderVersion() == 3);

    auto msg = fac->CreateReceiveMessage(hdr);
    REQUIRE(msg.IsNotNull());
    const std::size_t body_size =
        static_cast<std::size_t>(msg->GetPackBodySize());
    std::memcpy(msg->GetPackBodyPointer(),
                packed.data() + 58, body_size);
    msg->Unpack();

    std::string role;
    REQUIRE(msg->GetMetaDataElement("role", role));
    REQUIRE(role == "probe");
}

// -------- Test (3): GetMetaData enumerates in deterministic (sorted)
// order. Consumers occasionally iterate the whole map to relay
// metadata onward (e.g. PLUS's re-broadcast to downstream viewers);
// stable order avoids flaky tests and reproducible-bug ammunition.
void test_metadata_iteration_order() {
    auto tx = igtl::TransformMessage::New();
    tx->SetHeaderVersion(2);
    tx->SetMetaDataElement("zebra", IANA_TYPE_US_ASCII, "z");
    tx->SetMetaDataElement("alpha", IANA_TYPE_US_ASCII, "a");
    tx->SetMetaDataElement("mike",  IANA_TYPE_US_ASCII, "m");
    const auto& mp = tx->GetMetaData();
    std::vector<std::string> keys;
    for (const auto& kv : mp) keys.push_back(kv.first);
    REQUIRE(keys.size() == 3);
    REQUIRE(keys[0] == "alpha");
    REQUIRE(keys[1] == "mike");
    REQUIRE(keys[2] == "zebra");
}

}  // namespace

int main() {
    test_metadata_roundtrip_v2();
    test_header_version_3_accepted();
    test_metadata_iteration_order();

    if (g_fail_count > 0) {
        std::fprintf(stderr, "\nv3_metadata_audit: %d FAIL(s)\n",
                     g_fail_count);
        return 1;
    }
    std::fprintf(stderr, "\nv3_metadata_audit: all OK\n");
    return 0;
}
