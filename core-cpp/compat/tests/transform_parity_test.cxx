// Byte-exact parity test: pack a TRANSFORM the same way through
// our shim and through upstream's library; compare the wire bytes.
// This is the acceptance gate that proves the shim is compatible
// at the bit level, not just API-shape-ishly.
//
// Build guarded in CMakeLists: compiled only when the upstream
// library is available at corpus-tools/reference-libs/...

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

// Our shim:
#include "igtl/igtlMath.h"
#include "igtl/igtlTransformMessage.h"

// Upstream:
#include "../../corpus-tools/reference-libs/openigtlink-upstream/Source/igtlTransformMessage.h"
#include "../../corpus-tools/reference-libs/openigtlink-upstream/Source/igtlMath.h"

namespace {

int g_fail_count = 0;

#define REQUIRE(expr) do {                                            \
    if (!(expr)) {                                                    \
        std::fprintf(stderr, "  FAIL: %s:%d  %s\n",                   \
                     __FILE__, __LINE__, #expr);                      \
        ++g_fail_count;                                               \
    }                                                                 \
} while (0)

// Fill both messages with the same fields; compare the packed
// wire bytes. Bytes 34..42 (the timestamp) must be identical —
// we set both to the same value. Same for version, type, device.
void pack_parity(unsigned short version, bool with_metadata) {
    std::fprintf(stderr,
        "pack_parity(version=%u, metadata=%d)\n",
        version, (int)with_metadata);

    // ---- our shim ----
    auto ours = igtl::TransformMessage::New();
    ours->SetHeaderVersion(version);
    ours->SetDeviceName("Tracker_01");
    ours->SetTimeStamp(1718455896u, 0xC000'0000u);
    ours->SetMessageID(42);
    igtl::Matrix4x4 m_ours;
    igtl::IdentityMatrix(m_ours);
    m_ours[0][3] = 1.5f;
    m_ours[1][3] = 2.5f;
    m_ours[2][3] = 3.5f;
    ours->SetMatrix(m_ours);
    if (with_metadata) {
        ours->SetMetaDataElement("PatientID",
            igtl::IANA_TYPE_US_ASCII, "P-001");
    }
    ours->Pack();

    // ---- upstream ----
    ::igtl::TransformMessage::Pointer up = ::igtl::TransformMessage::New();
    up->SetHeaderVersion(version);
    up->SetDeviceName("Tracker_01");
    up->SetTimeStamp(1718455896u, 0xC000'0000u);
    up->SetMessageID(42);
    ::igtl::Matrix4x4 m_up;
    ::igtl::IdentityMatrix(m_up);
    m_up[0][3] = 1.5f;
    m_up[1][3] = 2.5f;
    m_up[2][3] = 3.5f;
    up->SetMatrix(m_up);
    if (with_metadata) {
        up->SetMetaDataElement("PatientID",
            ::igtl::IANA_TYPE_US_ASCII, "P-001");
    }
    up->Pack();

    // ---- byte compare ----
    const auto our_size = ours->GetPackSize();
    const auto up_size  = up->GetPackSize();
    REQUIRE(our_size == up_size);
    if (our_size != up_size) {
        std::fprintf(stderr,
            "    our_size=%llu up_size=%llu\n",
            (unsigned long long)our_size,
            (unsigned long long)up_size);
        return;
    }
    const auto* our_bytes = static_cast<const std::uint8_t*>(
        ours->GetPackPointer());
    const auto* up_bytes  = static_cast<const std::uint8_t*>(
        up->GetPackPointer());
    bool equal = std::memcmp(our_bytes, up_bytes, our_size) == 0;
    REQUIRE(equal);
    if (!equal) {
        std::fprintf(stderr, "    byte mismatch; first-diff dump:\n");
        for (std::uint64_t i = 0; i < our_size; ++i) {
            if (our_bytes[i] != up_bytes[i]) {
                std::fprintf(stderr,
                    "    offset %llu: ours=0x%02x up=0x%02x\n",
                    (unsigned long long)i,
                    our_bytes[i], up_bytes[i]);
                if (i >= 8) break;  // cap dump length
            }
        }
    }
}

// Round-trip: take upstream's packed bytes, feed them into our
// shim's Unpack, check we recover the same matrix.
void unpack_parity() {
    std::fprintf(stderr, "unpack_parity\n");

    ::igtl::TransformMessage::Pointer up = ::igtl::TransformMessage::New();
    up->SetHeaderVersion(2);
    up->SetDeviceName("Source");
    up->SetTimeStamp(12345, 0);
    ::igtl::Matrix4x4 m_up;
    ::igtl::IdentityMatrix(m_up);
    m_up[0][0] = 0.0f; m_up[0][1] = -1.0f;
    m_up[1][0] = 1.0f; m_up[1][1] = 0.0f;
    m_up[0][3] = 10.0f;
    m_up[1][3] = 20.0f;
    m_up[2][3] = 30.0f;
    up->SetMatrix(m_up);
    up->Pack();

    const auto size = up->GetPackSize();
    const auto* bytes = static_cast<const std::uint8_t*>(
        up->GetPackPointer());

    // Feed the bytes through our shim.
    auto ours = igtl::TransformMessage::New();
    ours->InitPack();  // clears internal buffer; now alloc full size
    // Poke the wire in directly: a real consumer does it via the
    // socket recv path, but here we test the Unpack pipeline in
    // isolation.
    std::memcpy(ours->GetPackPointer(), bytes,
                58);  // header first
    int s1 = ours->Unpack(0);
    REQUIRE((s1 & igtl::UNPACK_HEADER) != 0);
    REQUIRE(ours->GetBodySizeToRead() + 58 == size);

    // Now size the buffer for the body and feed that.
    ours->AllocatePack();
    std::memcpy(static_cast<std::uint8_t*>(ours->GetPackPointer()) + 58,
                bytes + 58, size - 58);
    int s2 = ours->Unpack(1);  // with CRC check
    REQUIRE((s2 & igtl::UNPACK_HEADER) != 0);
    REQUIRE((s2 & igtl::UNPACK_BODY)   != 0);

    igtl::Matrix4x4 m_ours;
    ours->GetMatrix(m_ours);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 4; ++j)
            REQUIRE(m_ours[i][j] == m_up[i][j]);
    REQUIRE(ours->GetDeviceName() == "Source");
}

}  // namespace

int main() {
    pack_parity(1, /*metadata=*/false);
    pack_parity(2, /*metadata=*/false);
    pack_parity(2, /*metadata=*/true);
    unpack_parity();

    if (g_fail_count == 0) {
        std::fprintf(stderr, "transform_parity_test: all passed\n");
        return 0;
    }
    std::fprintf(stderr, "transform_parity_test: %d failure(s)\n",
                 g_fail_count);
    return 1;
}
