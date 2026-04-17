// Shim-side parity emitter. Links ONLY against our libigtl_compat +
// libigtl_runtime — no upstream symbols in scope. Packs a message
// with fixed canonical inputs and writes the wire bytes to stdout.
//
// The companion `emitter_upstream` does the same thing using
// upstream's library. A ctest diffs their stdouts — any mismatch
// is a real, ODR-safe parity defect.
//
// Usage: emitter_shim <case_name>
//   case_name ∈ {transform_v1, transform_v2, transform_v2_meta,
//                get_status_v2, stt_tdata_v2}

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include "igtl/igtlGetStatusMessage.h"
#include "igtl/igtlMath.h"
#include "igtl/igtlStartTrackingDataMessage.h"
#include "igtl/igtlTransformMessage.h"

static int emit_transform(unsigned short version, bool with_meta) {
    auto msg = igtl::TransformMessage::New();
    msg->SetHeaderVersion(version);
    msg->SetDeviceName("Tracker_01");
    msg->SetTimeStamp(1718455896u, 0xC000'0000u);
    msg->SetMessageID(42);
    igtl::Matrix4x4 m;
    igtl::IdentityMatrix(m);
    m[0][3] = 1.5f;
    m[1][3] = 2.5f;
    m[2][3] = 3.5f;
    msg->SetMatrix(m);
    if (with_meta) {
        msg->SetMetaDataElement("PatientID",
            IANA_TYPE_US_ASCII, "P-001");
    }
    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

static int emit_get_status() {
    auto msg = igtl::GetStatusMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("Probe");
    msg->SetTimeStamp(1718455896u, 0);
    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

// For STT_TDATA we only compare the 58-byte header — upstream's
// StartTrackingDataMessage carries a body we don't pack.
static int emit_stt_tdata_header_only() {
    auto msg = igtl::StartTrackingDataMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("TDClient");
    msg->SetTimeStamp(500, 0);
    msg->Pack();
    // Only the first 58 bytes.
    ::write(1, msg->GetPackPointer(), 58);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    const std::string c(argv[1]);
    if (c == "transform_v1")       return emit_transform(1, false);
    if (c == "transform_v2")       return emit_transform(2, false);
    if (c == "transform_v2_meta")  return emit_transform(2, true);
    if (c == "get_status_v2")      return emit_get_status();
    if (c == "stt_tdata_v2")       return emit_stt_tdata_header_only();
    std::fprintf(stderr, "unknown case: %s\n", argv[1]);
    return 2;
}
