// Upstream-side parity emitter. Symmetric to emitter_shim; links
// ONLY against libOpenIGTLink.a. No shim symbols in scope.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include "igtlBindMessage.h"
#include "igtlCapabilityMessage.h"
#include "igtlColorTableMessage.h"
#include "igtlCommandMessage.h"
#include "igtlImageMessage.h"
#include "igtlImageMetaMessage.h"
#include "igtlLabelMetaMessage.h"
#include "igtlNDArrayMessage.h"
#include "igtlPolyDataMessage.h"
#include "igtlQueryMessage.h"
#include "igtlSensorMessage.h"
#include "igtlTimeStamp.h"
#include "igtlTrajectoryMessage.h"
#include "igtlMath.h"
#include "igtlPointMessage.h"
#include "igtlPositionMessage.h"
#include "igtlQuaternionTrackingDataMessage.h"
#include "igtlStatusMessage.h"        // GetStatusMessage
#include "igtlStringMessage.h"
#include "igtlTrackingDataMessage.h"
#include "igtlTransformMessage.h"

static void fill_matrix(igtl::Matrix4x4& m, int kind) {
    igtl::IdentityMatrix(m);
    if (kind == 0) {
        m[0][3] = 1.5f;
        m[1][3] = 2.5f;
        m[2][3] = 3.5f;
    } else {
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

static int emit_command() {
    auto msg = igtl::CommandMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("Workstation");
    msg->SetTimeStamp(1718455896u, 0);
    msg->SetCommandId(0xDEADBEEF);
    msg->SetCommandName("StartScan");
    msg->SetContentEncoding(106);
    msg->SetCommandContent("protocol=T2W;tr=4000;te=105");
    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

static int emit_query() {
    auto msg = igtl::QueryMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("Slicer");
    msg->SetTimeStamp(1718455896u, 0);
    // Upstream's QueryMessage doesn't expose SetQueryID publicly in
    // all versions; skip that field and rely on the default 0.
    msg->SetDataType("IMAGE");
    msg->SetDeviceUID("MR_Scanner_01");
    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

static int emit_imgmeta() {
    auto msg = igtl::ImageMetaMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("ImageDB");
    msg->SetTimeStamp(1718455896u, 0);
    for (int i = 0; i < 2; ++i) {
        auto e = igtl::ImageMetaElement::New();
        e->SetName(i == 0 ? "Patient A T1" : "Patient A T2");
        e->SetDeviceName(i == 0 ? "T1_01" : "T2_01");
        e->SetModality("MR");
        e->SetPatientName("Doe, Jane");
        e->SetPatientID("P-0421");
        auto ts = igtl::TimeStamp::New();
        ts->SetTime(1718455000u + i, 0);
        igtl::TimeStamp::Pointer tsp = ts;
        e->SetTimeStamp(tsp);
        igtlUint16 sz[3] = {256, 256, 128};
        e->SetSize(sz);
        e->SetScalarType(5);
        msg->AddImageMetaElement(e);
    }
    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

static int emit_lbmeta() {
    auto msg = igtl::LabelMetaMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("LabelDB");
    msg->SetTimeStamp(1718455896u, 0);
    auto e = igtl::LabelMetaElement::New();
    e->SetName("Liver Segmentation");
    e->SetDeviceName("LabelImg_01");
    e->SetLabel(1);
    e->SetRGBA(255, 100, 50, 255);
    e->SetSize(256, 256, 64);
    e->SetOwner("T1_01");
    msg->AddLabelMetaElement(e);
    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

static int emit_traj() {
    auto msg = igtl::TrajectoryMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("Planner");
    msg->SetTimeStamp(1718455896u, 0);
    auto e = igtl::TrajectoryElement::New();
    e->SetName("Biopsy_1");
    e->SetGroupName("Biopsy");
    e->SetType(igtl::TrajectoryElement::TYPE_ENTRY_TARGET);
    e->SetRGBA(255, 0, 0, 255);
    e->SetEntryPosition(10.0f, 20.0f, 5.0f);
    e->SetTargetPosition(15.0f, 22.0f, 50.0f);
    e->SetRadius(1.25f);
    e->SetOwner("HeadMRI");
    msg->AddTrajectoryElement(e);
    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

static int emit_ndarray() {
    auto msg = igtl::NDArrayMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("Compute_01");
    msg->SetTimeStamp(1718455896u, 0);
    // Upstream's Array<T> is a template without ::New() — it's
    // owned via raw pointer and not ref-counted like ArrayBase.
    auto* arr = new igtl::Array<float>;
    igtl::ArrayBase::IndexType sz = {2, 3};
    arr->SetSize(sz);
    float vals[6] = {1.5f, -2.25f, 0.0f, 100.0f, 3.14f, -0.5f};
    arr->SetArray(vals);
    msg->SetArray(igtl::NDArrayMessage::TYPE_FLOAT32, arr);
    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

static int emit_colort() {
    auto msg = igtl::ColorTableMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("LUT");
    msg->SetTimeStamp(1718455896u, 0);
    msg->SetIndexTypeToUint8();
    msg->SetMapTypeToUint8();
    // Upstream's API requires AllocateTable() before writing;
    // PackContent casts m_ColorTableHeader which is NULL until
    // AllocateTable runs. Missing this call segfaults.
    msg->AllocateTable();
    auto* tbl = static_cast<std::uint8_t*>(msg->GetTablePointer());
    for (int i = 0; i < 256; ++i) tbl[i] = static_cast<std::uint8_t>(i);
    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

static int emit_polydata_empty() {
    auto msg = igtl::PolyDataMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("Surface");
    msg->SetTimeStamp(1718455896u, 0);
    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

static int emit_polydata_tri() {
    auto msg = igtl::PolyDataMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("Surface");
    msg->SetTimeStamp(1718455896u, 0);

    auto pts = igtl::PolyDataPointArray::New();
    pts->AddPoint(0.0f, 0.0f, 0.0f);
    pts->AddPoint(1.0f, 0.0f, 0.0f);
    pts->AddPoint(0.0f, 1.0f, 0.0f);
    pts->AddPoint(1.0f, 1.0f, 0.0f);
    msg->SetPoints(pts.GetPointer());

    auto polys = igtl::PolyDataCellArray::New();
    igtlUint32 tri0[3] = {0, 1, 2};
    igtlUint32 tri1[3] = {1, 3, 2};
    polys->AddCell(3, tri0);
    polys->AddCell(3, tri1);
    msg->SetPolygons(polys.GetPointer());

    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

static int emit_capability() {
    auto msg = igtl::CapabilityMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("CapRecorder");
    msg->SetTimeStamp(1718455896u, 0);
    std::vector<std::string> types = {"TRANSFORM", "STATUS", "STRING",
                                      "IMAGE", "POINT"};
    msg->SetTypes(types);
    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

static int emit_sensor() {
    auto msg = igtl::SensorMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("Pressure_01");
    msg->SetTimeStamp(1718455896u, 0);
    msg->SetLength(3);
    igtlFloat64 samples[3] = {101.325, 37.2, 9.81};
    msg->SetValue(samples);
    msg->SetUnit(0x0123456789ABCDEFULL);
    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

static int emit_image() {
    auto msg = igtl::ImageMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("MR_Scanner");
    msg->SetTimeStamp(1718455896u, 0);

    msg->SetDimensions(4, 3, 2);
    msg->SetNumComponents(1);
    msg->SetScalarTypeToUint16();
    msg->SetEndian(igtl::ImageMessage::ENDIAN_BIG);
    msg->SetCoordinateSystem(igtl::ImageMessage::COORDINATE_RAS);
    msg->SetSpacing(0.5f, 0.75f, 2.0f);
    msg->SetOrigin(-10.0f, 20.5f, 100.0f);

    igtl::Matrix4x4 m; igtl::IdentityMatrix(m);
    m[0][0] = 0.0f; m[0][1] = -1.0f;
    m[1][0] = 1.0f; m[1][1] =  0.0f;
    m[0][3] = -10.0f; m[1][3] = 20.5f; m[2][3] = 100.0f;
    msg->SetMatrix(m);

    msg->AllocateScalars();
    auto* pix = static_cast<std::uint16_t*>(msg->GetScalarPointer());
    for (int i = 0; i < 24; ++i) {
        const std::uint16_t v = static_cast<std::uint16_t>(
            0x1000 + i * 37);
        auto* b = reinterpret_cast<std::uint8_t*>(pix + i);
        b[0] = static_cast<std::uint8_t>(v >> 8);
        b[1] = static_cast<std::uint8_t>(v & 0xff);
    }

    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

static int emit_qtdata() {
    auto msg = igtl::QuaternionTrackingDataMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("Tracker_Q");
    msg->SetTimeStamp(1718455896u, 0);

    auto make = [](const char* name, igtlUint8 type,
                   float px, float py, float pz,
                   float qx, float qy, float qz, float qw) {
        auto e = igtl::QuaternionTrackingDataElement::New();
        e->SetName(name);
        e->SetType(type);
        e->SetPosition(px, py, pz);
        e->SetQuaternion(qx, qy, qz, qw);
        return e;
    };

    auto e1 = make("Probe",
        igtl::QuaternionTrackingDataElement::TYPE_6D,
        10.0f, 20.0f, 30.0f, 0.0f, 0.0f, 0.7071068f, 0.7071068f);
    auto e2 = make("Reference",
        igtl::QuaternionTrackingDataElement::TYPE_TRACKER,
        5.25f, -7.5f, 42.0f, 0.0f, 0.0f, 0.0f, 1.0f);

    msg->AddQuaternionTrackingDataElement(e1);
    msg->AddQuaternionTrackingDataElement(e2);
    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

static int emit_tdata() {
    auto msg = igtl::TrackingDataMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("Tracker_B");
    msg->SetTimeStamp(1718455896u, 0);

    auto make = [](const char* name, igtlUint8 type,
                   const igtl::Matrix4x4& m) {
        auto e = igtl::TrackingDataElement::New();
        e->SetName(name);
        e->SetType(type);
        auto& mm = const_cast<igtl::Matrix4x4&>(m);
        e->SetMatrix(mm);
        return e;
    };

    igtl::Matrix4x4 m1, m2;
    igtl::IdentityMatrix(m1); igtl::IdentityMatrix(m2);
    m1[0][0] =  0.0f; m1[0][1] = -1.0f;
    m1[1][0] =  1.0f; m1[1][1] =  0.0f;
    m1[0][3] = 10.0f; m1[1][3] = 20.0f; m1[2][3] = 30.0f;

    m2[0][3] = 5.25f; m2[1][3] = -7.5f; m2[2][3] = 42.0f;

    auto e1 = make("Probe", igtl::TrackingDataElement::TYPE_6D, m1);
    auto e2 = make("Reference",
                   igtl::TrackingDataElement::TYPE_TRACKER, m2);
    msg->AddTrackingDataElement(e1);
    msg->AddTrackingDataElement(e2);
    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

static int emit_point() {
    auto msg = igtl::PointMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("Planner");
    msg->SetTimeStamp(1718455896u, 0);

    auto make_pt = [](const char* name, const char* group,
                      igtlUint8 r, igtlUint8 g, igtlUint8 b,
                      igtlUint8 a, float x, float y, float z,
                      float rad, const char* owner) {
        auto p = igtl::PointElement::New();
        p->SetName(name);
        p->SetGroupName(group);
        p->SetRGBA(r, g, b, a);
        p->SetPosition(x, y, z);
        p->SetRadius(rad);
        p->SetOwner(owner);
        return p;
    };

    auto p1 = make_pt("Nasion", "Fiducial", 255, 0, 0, 255,
                      12.5f, -4.5f, 200.75f, 2.0f, "HeadMRI");
    auto p2 = make_pt("Target_1", "Target", 0, 255, 0, 255,
                      -15.25f, 8.0f, 180.0f, 1.5f, "HeadMRI");
    auto p3 = make_pt("Landmark_A", "Landmark", 0, 0, 255, 128,
                      0.0f, 0.0f, 0.0f, 0.0f, "");

    msg->AddPointElement(p1);
    msg->AddPointElement(p2);
    msg->AddPointElement(p3);
    msg->Pack();
    ::write(1, msg->GetPackPointer(),
            static_cast<size_t>(msg->GetPackSize()));
    return 0;
}

static int emit_position(int pack_type) {
    auto msg = igtl::PositionMessage::New();
    msg->SetHeaderVersion(2);
    msg->SetDeviceName("Probe_B");
    msg->SetTimeStamp(1718455896u, 0);
    msg->SetPackType(pack_type);
    msg->SetPosition(7.5f, -2.25f, 13.0f);
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
    msg->SetEncoding(106);
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
    if (c == "point_v2")           return emit_point();
    if (c == "tdata_v2")           return emit_tdata();
    if (c == "qtdata_v2")          return emit_qtdata();
    if (c == "image_v2")           return emit_image();
    if (c == "capability_v2")      return emit_capability();
    if (c == "sensor_v2")          return emit_sensor();
    if (c == "command_v2")         return emit_command();
    if (c == "query_v2")           return emit_query();
    if (c == "imgmeta_v2")         return emit_imgmeta();
    if (c == "lbmeta_v2")          return emit_lbmeta();
    if (c == "traj_v2")            return emit_traj();
    if (c == "ndarray_v2")         return emit_ndarray();
    if (c == "colort_v2")          return emit_colort();
    if (c == "polydata_empty_v2")  return emit_polydata_empty();
    if (c == "polydata_tri_v2")    return emit_polydata_tri();
    if (c == "stt_tdata_v2")       return emit_stt_tdata_header_only();
    std::fprintf(stderr, "unknown case: %s\n", argv[1]);
    return 2;
}
