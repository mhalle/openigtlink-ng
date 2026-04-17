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
#include "igtl/igtlPositionMessage.h"
#include "igtl/igtlStatusMessage.h"
#include "igtl/igtlStringMessage.h"
#include "igtl/igtlTransformMessage.h"

// matrix_kind: 0 = identity + translation (canonical simple case)
//              1 = 90° Z-rotation + translation (non-identity R).
// The non-identity case distinguishes column-major-3x4 from the
// row-major-3x3-then-translation alternative that an identity
// rotation happens to encode identically.
static void fill_matrix(igtl::Matrix4x4& m, int kind) {
    igtl::IdentityMatrix(m);
    if (kind == 0) {
        m[0][3] = 1.5f;
        m[1][3] = 2.5f;
        m[2][3] = 3.5f;
    } else {
        // Rz(90°):  [0 -1 0]    plus translation (10, 20, 30)
        //           [1  0 0]
        //           [0  0 1]
        m[0][0] =  0.0f; m[0][1] = -1.0f; m[0][2] = 0.0f;
        m[1][0] =  1.0f; m[1][1] =  0.0f; m[1][2] = 0.0f;
        m[2][0] =  0.0f; m[2][1] =  0.0f; m[2][2] = 1.0f;
        m[0][3] = 10.0f;
        m[1][3] = 20.0f;
        m[2][3] = 30.0f;
    }
}

static int emit_transform(unsigned short version, bool with_meta,
                          int matrix_kind) {
    auto msg = igtl::TransformMessage::New();
    msg->SetHeaderVersion(version);
    msg->SetDeviceName("Tracker_01");
    msg->SetTimeStamp(1718455896u, 0xC000'0000u);
    msg->SetMessageID(42);
    igtl::Matrix4x4 m;
    fill_matrix(m, matrix_kind);
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
// POSITION variants: 12 B (POSITION_ONLY), 24 B (WITH_QUATERNION3),
// 28 B (ALL). Non-trivial position + non-identity rotation.
static int emit_position(int pack_type) {
    auto msg = igtl::PositionMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("Probe_B");
    msg->SetTimeStamp(1718455896u, 0);
    msg->SetPackType(pack_type);
    msg->SetPosition(7.5f, -2.25f, 13.0f);
    // Asymmetric quaternion: Rz(60°) ≈ (0, 0, 0.5, 0.866025...)
    msg->SetQuaternion(0.0f, 0.0f, 0.5f, 0.8660254f);
    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

static int emit_string() {
    auto msg = igtl::StringMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("Logger");
    msg->SetTimeStamp(1718455896u, 0);
    msg->SetEncoding(106);  // UTF-8
    msg->SetString("temperature out of range: 38.2°C");
    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

static int emit_status() {
    auto msg = igtl::StatusMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("Probe");
    msg->SetTimeStamp(1718455896u, 0);
    msg->SetCode(igtl::StatusMessage::STATUS_NOT_READY);
    msg->SetSubCode(static_cast<igtlInt64>(0x1122334455667788LL));
    msg->SetErrorName("INIT_FAIL");
    msg->SetStatusString("scanner not warmed up yet");
    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

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
    if (c == "transform_v1")          return emit_transform(1, false, 0);
    if (c == "transform_v2")          return emit_transform(2, false, 0);
    if (c == "transform_v2_meta")     return emit_transform(2, true, 0);
    if (c == "transform_v2_rot90z")   return emit_transform(2, false, 1);
    if (c == "get_status_v2")      return emit_get_status();
    if (c == "status_v2")          return emit_status();
    if (c == "string_v2")          return emit_string();
    if (c == "position_only_v2")   return emit_position(
                                       igtl::PositionMessage::POSITION_ONLY);
    if (c == "position_quat3_v2")  return emit_position(
                                       igtl::PositionMessage::WITH_QUATERNION3);
    if (c == "position_all_v2")    return emit_position(
                                       igtl::PositionMessage::ALL);
    if (c == "stt_tdata_v2")       return emit_stt_tdata_header_only();
    std::fprintf(stderr, "unknown case: %s\n", argv[1]);
    return 2;
}
