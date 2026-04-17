// upstream_generator_cli — emits wire messages produced by the pinned
// upstream OpenIGTLink library's Pack() path.
//
// Purpose: feed the `oigtl-corpus fuzz roundtrip` runner vectors that
// are, by construction, "what upstream emits on the wire." Round-
// tripping these through our four codecs gives us conformance
// coverage on a class the consumer-only 6th oracle cannot see — bugs
// in our *pack* path would produce bytes upstream can't parse, and
// neither side of that exchange would ever surface as a cross-oracle
// disagreement in the existing fuzzer stream.
//
// Protocol:
//   stdin  — ignored (generator, not a consumer).
//   stdout — one lowercase hex wire message per line. EOF after
//            --count messages.
//   stderr — progress / errors.
//
// Args (all optional):
//   --count N    (default 1000)
//   --seed S     (default 1)
//   --type T     restrict to one type_id; omit to round-robin.
//
// Coverage:
//   TRANSFORM, POSITION, STATUS, STRING, SENSOR, POINT, IMAGE.
//   Other types upstream ships (BIND / POLYDATA / NDARRAY / TDATA /
//   COLORT / QTDATA / TRAJ / CAPABILITY / IMGMETA / LBMETA / COMMAND)
//   are left out of this first pass — their setters are more involved
//   and the 7 types here already exercise the common field shapes.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "igtlBindMessage.h"
#include "igtlCapabilityMessage.h"
#include "igtlColorTableMessage.h"
#include "igtlImageMessage.h"
#include "igtlMessageBase.h"
#include "igtlNDArrayMessage.h"
#include "igtlPointMessage.h"
#include "igtlPolyDataMessage.h"
#include "igtlPositionMessage.h"
#include "igtlQuaternionTrackingDataMessage.h"
#include "igtlSensorMessage.h"
#include "igtlStatusMessage.h"
#include "igtlStringMessage.h"
#include "igtlTrackingDataMessage.h"
#include "igtlTrajectoryMessage.h"
#include "igtlTransformMessage.h"

namespace {

// ---------------------------------------------------------------------------
// Hex emitter — lowercase, no separators, matches the oracle CLIs'
// stdin protocol exactly.
// ---------------------------------------------------------------------------
void emit_hex(const std::uint8_t* data, std::size_t len) {
    static const char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2 + 1);
    for (std::size_t i = 0; i < len; ++i) {
        out.push_back(kHex[(data[i] >> 4) & 0xF]);
        out.push_back(kHex[data[i] & 0xF]);
    }
    out.push_back('\n');
    std::cout.write(out.data(), static_cast<std::streamsize>(out.size()));
}

// ---------------------------------------------------------------------------
// PRNG-driven per-type generators. Each returns wire bytes ready to
// emit; any upstream Pack()-or-earlier failure returns empty and the
// message is skipped.
// ---------------------------------------------------------------------------

// Names are ASCII-only, within IGTL_HEADER_NAME_SIZE-1 (19).
std::string random_ascii_name(std::mt19937_64& rng, std::size_t max_len) {
    std::uniform_int_distribution<std::size_t> lendist(1, max_len);
    std::uniform_int_distribution<int> chardist('A', 'Z');
    std::size_t n = lendist(rng);
    std::string out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(static_cast<char>(chardist(rng)));
    }
    return out;
}

void apply_common(igtl::MessageBase* msg, std::mt19937_64& rng) {
    msg->SetDeviceName(random_ascii_name(rng, 19).c_str());
    std::uniform_int_distribution<unsigned> sec(0, 0x7FFFFFFF);
    std::uniform_int_distribution<unsigned> frac(0, 0xFFFFFFFF);
    msg->SetTimeStamp(sec(rng), frac(rng));
}

std::vector<std::uint8_t> gen_transform(std::mt19937_64& rng) {
    auto msg = igtl::TransformMessage::New();
    apply_common(msg, rng);
    igtl::Matrix4x4 m;
    igtl::IdentityMatrix(m);
    std::uniform_real_distribution<float> f(-1000.0f, 1000.0f);
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) m[r][c] = f(rng);
    }
    m[3][0] = m[3][1] = m[3][2] = 0.0f;
    m[3][3] = 1.0f;
    msg->SetMatrix(m);
    msg->Pack();
    auto* p = static_cast<const std::uint8_t*>(msg->GetBufferPointer());
    return {p, p + msg->GetBufferSize()};
}

std::vector<std::uint8_t> gen_position(std::mt19937_64& rng) {
    auto msg = igtl::PositionMessage::New();
    apply_common(msg, rng);
    std::uniform_real_distribution<float> f(-1000.0f, 1000.0f);
    msg->SetPosition(f(rng), f(rng), f(rng));
    // PackType ∈ {POSITION_ONLY, WITH_QUATERNION3, ALL} → body
    // sizes {12, 24, 28}. Let upstream pick per-size.
    std::uniform_int_distribution<int> packtype_dist(1, 3);
    int pt = packtype_dist(rng);
    msg->SetPackType(pt);
    if (pt != igtl::PositionMessage::POSITION_ONLY) {
        msg->SetQuaternion(f(rng), f(rng), f(rng), f(rng));
    }
    msg->Pack();
    auto* p = static_cast<const std::uint8_t*>(msg->GetBufferPointer());
    return {p, p + msg->GetBufferSize()};
}

std::vector<std::uint8_t> gen_status(std::mt19937_64& rng) {
    auto msg = igtl::StatusMessage::New();
    apply_common(msg, rng);
    std::uniform_int_distribution<int> code(0, 100);
    std::uniform_int_distribution<std::int64_t> sub(-1000, 1000);
    msg->SetCode(code(rng));
    msg->SetSubCode(sub(rng));
    msg->SetErrorName(random_ascii_name(rng, 19).c_str());
    std::string str = random_ascii_name(rng, 80);
    msg->SetStatusString(str.c_str());
    msg->Pack();
    auto* p = static_cast<const std::uint8_t*>(msg->GetBufferPointer());
    return {p, p + msg->GetBufferSize()};
}

std::vector<std::uint8_t> gen_string(std::mt19937_64& rng) {
    auto msg = igtl::StringMessage::New();
    apply_common(msg, rng);
    msg->SetEncoding(3);  // IANA 3 = US-ASCII
    std::uniform_int_distribution<std::size_t> n(0, 200);
    std::string s = random_ascii_name(rng, n(rng) + 1);
    msg->SetString(s.c_str());
    msg->Pack();
    auto* p = static_cast<const std::uint8_t*>(msg->GetBufferPointer());
    return {p, p + msg->GetBufferSize()};
}

std::vector<std::uint8_t> gen_sensor(std::mt19937_64& rng) {
    auto msg = igtl::SensorMessage::New();
    apply_common(msg, rng);
    std::uniform_int_distribution<unsigned> n(0, 16);
    const unsigned larray = n(rng);
    msg->SetLength(larray);
    std::uniform_real_distribution<double> f(-1.0, 1.0);
    for (unsigned i = 0; i < larray; ++i) {
        msg->SetValue(i, f(rng));
    }
    msg->Pack();
    auto* p = static_cast<const std::uint8_t*>(msg->GetBufferPointer());
    return {p, p + msg->GetBufferSize()};
}

std::vector<std::uint8_t> gen_point(std::mt19937_64& rng) {
    auto msg = igtl::PointMessage::New();
    apply_common(msg, rng);
    std::uniform_int_distribution<int> n(1, 4);
    std::uniform_real_distribution<float> f(-1000.0f, 1000.0f);
    std::uniform_int_distribution<int> byte(0, 255);
    for (int i = 0, m = n(rng); i < m; ++i) {
        auto pt = igtl::PointElement::New();
        pt->SetName(random_ascii_name(rng, 15).c_str());
        pt->SetGroupName(random_ascii_name(rng, 15).c_str());
        std::uint8_t rgba[4] = {
            static_cast<std::uint8_t>(byte(rng)),
            static_cast<std::uint8_t>(byte(rng)),
            static_cast<std::uint8_t>(byte(rng)),
            static_cast<std::uint8_t>(byte(rng))};
        pt->SetRGBA(rgba);
        pt->SetPosition(f(rng), f(rng), f(rng));
        pt->SetRadius(static_cast<float>(std::abs(f(rng))));
        pt->SetOwner(random_ascii_name(rng, 15).c_str());
        msg->AddPointElement(pt);
    }
    msg->Pack();
    auto* p = static_cast<const std::uint8_t*>(msg->GetBufferPointer());
    return {p, p + msg->GetBufferSize()};
}

std::vector<std::uint8_t> gen_image(std::mt19937_64& rng) {
    auto msg = igtl::ImageMessage::New();
    apply_common(msg, rng);
    // Keep dimensions small; the purpose is coverage, not throughput.
    std::uniform_int_distribution<int> dim(1, 8);
    const int sx = dim(rng), sy = dim(rng), sz = dim(rng);
    // scalar_type ∈ {2,3,4,5,6,7,10,11} — all IMAGE-legal
    static const int kScalars[] = {
        igtl::ImageMessage::TYPE_INT8,
        igtl::ImageMessage::TYPE_UINT8,
        igtl::ImageMessage::TYPE_INT16,
        igtl::ImageMessage::TYPE_UINT16,
        igtl::ImageMessage::TYPE_INT32,
        igtl::ImageMessage::TYPE_UINT32,
        igtl::ImageMessage::TYPE_FLOAT32,
        igtl::ImageMessage::TYPE_FLOAT64,
    };
    std::uniform_int_distribution<std::size_t> st(0, sizeof(kScalars)/sizeof(kScalars[0]) - 1);
    std::uniform_int_distribution<int> nc(1, 4);
    std::uniform_int_distribution<int> endian(1, 3);
    std::uniform_int_distribution<int> coord(1, 2);
    msg->SetDimensions(sx, sy, sz);
    msg->SetSpacing(1.0f, 1.0f, 1.0f);
    msg->SetOrigin(0.0f, 0.0f, 0.0f);
    msg->SetScalarType(kScalars[st(rng)]);
    msg->SetNumComponents(nc(rng));
    msg->SetEndian(endian(rng));
    msg->SetCoordinateSystem(coord(rng));
    msg->AllocateScalars();
    // Fill the pixel region with PRNG bytes so round-trip has to
    // preserve the exact payload.
    std::uint8_t* scalars =
        static_cast<std::uint8_t*>(msg->GetScalarPointer());
    const std::size_t bytes = msg->GetImageSize();
    std::uniform_int_distribution<int> b(0, 255);
    for (std::size_t i = 0; i < bytes; ++i) scalars[i] = b(rng);
    msg->Pack();
    auto* p = static_cast<const std::uint8_t*>(msg->GetBufferPointer());
    return {p, p + msg->GetBufferSize()};
}

// NDARRAY — 1D float64 array. The upstream templated Array<T>
// + SetArray(TYPE, ArrayBase*) pattern (see igtlNDArrayMessageTest.cxx).
// Keep to 1 dimension with a small extent so the full set of
// invariant checks (scalar_type ∈ set, data == product(size) × bps)
// is exercised without blowing up payload size.
std::vector<std::uint8_t> gen_ndarray(std::mt19937_64& rng) {
    auto msg = igtl::NDArrayMessage::New();
    apply_common(msg, rng);
    std::uniform_int_distribution<std::size_t> n(1, 32);
    const std::size_t N = n(rng);
    igtl::Array<igtl_float64> array;
    igtl::ArrayBase::IndexType size(1);
    size[0] = static_cast<igtlUint16>(N);
    array.SetSize(size);
    std::vector<igtl_float64> buf(N);
    std::uniform_real_distribution<double> f(-1.0, 1.0);
    for (std::size_t i = 0; i < N; ++i) buf[i] = f(rng);
    array.SetArray(buf.data());
    msg->SetArray(igtl::NDArrayMessage::TYPE_FLOAT64, &array);
    msg->Pack();
    auto* p = static_cast<const std::uint8_t*>(msg->GetBufferPointer());
    return {p, p + msg->GetBufferSize()};
}

// COLORT — COLORTABLE with the short "COLORT" wire name upstream
// uses. Exercises all 6 valid (index_type, map_type) combinations
// uniformly; each trigger a different table size.
std::vector<std::uint8_t> gen_colort(std::mt19937_64& rng) {
    auto msg = igtl::ColorTableMessage::New();
    apply_common(msg, rng);
    static const int kIndex[] = {3, 5};      // uint8=256, uint16=65536
    static const int kMap[]   = {3, 5, 19};  // uint8=1, uint16=2, RGB=3
    std::uniform_int_distribution<std::size_t> idx_i(0, 1);
    std::uniform_int_distribution<std::size_t> map_i(0, 2);
    const int index_type = kIndex[idx_i(rng)];
    const int map_type   = kMap[map_i(rng)];
    msg->SetIndexType(index_type);
    msg->SetMapType(map_type);
    msg->AllocateTable();
    std::uint8_t* table = static_cast<std::uint8_t*>(msg->GetTablePointer());
    const std::size_t table_size =
        static_cast<std::size_t>(msg->GetColorTableSize());
    std::uniform_int_distribution<int> b(0, 255);
    for (std::size_t i = 0; i < table_size; ++i) {
        table[i] = static_cast<std::uint8_t>(b(rng));
    }
    msg->Pack();
    auto* p = static_cast<const std::uint8_t*>(msg->GetBufferPointer());
    return {p, p + msg->GetBufferSize()};
}

// BIND — container bundling 1-3 pre-packed children. Only the
// simplest child types (TRANSFORM, STATUS, SENSOR) to keep the
// generator deterministic and avoid nested variable-count surprises.
std::vector<std::uint8_t> gen_bind(std::mt19937_64& rng) {
    auto msg = igtl::BindMessage::New();
    apply_common(msg, rng);
    msg->Init();
    std::uniform_int_distribution<int> nchild(1, 3);
    const int n = nchild(rng);
    // Keep children in local Pointers so they outlive AppendChild.
    std::vector<igtl::MessageBase::Pointer> children;
    std::uniform_int_distribution<int> pick(0, 2);
    std::uniform_real_distribution<float> f(-100.0f, 100.0f);
    for (int i = 0; i < n; ++i) {
        igtl::MessageBase::Pointer child;
        switch (pick(rng)) {
            case 0: {
                auto t = igtl::TransformMessage::New();
                t->SetDeviceName(random_ascii_name(rng, 10).c_str());
                t->SetTimeStamp(0, 1);
                igtl::Matrix4x4 m;
                igtl::IdentityMatrix(m);
                for (int r = 0; r < 3; ++r)
                    for (int c = 0; c < 4; ++c) m[r][c] = f(rng);
                t->SetMatrix(m);
                t->Pack();
                child = t;
                break;
            }
            case 1: {
                auto s = igtl::StatusMessage::New();
                s->SetDeviceName(random_ascii_name(rng, 10).c_str());
                s->SetTimeStamp(0, 1);
                s->SetCode(0);
                s->SetSubCode(0);
                s->SetErrorName(random_ascii_name(rng, 10).c_str());
                s->SetStatusString(random_ascii_name(rng, 20).c_str());
                s->Pack();
                child = s;
                break;
            }
            default: {
                auto sen = igtl::SensorMessage::New();
                sen->SetDeviceName(random_ascii_name(rng, 10).c_str());
                sen->SetTimeStamp(0, 1);
                std::uniform_int_distribution<unsigned> m(0, 4);
                const unsigned larray = m(rng);
                sen->SetLength(larray);
                for (unsigned k = 0; k < larray; ++k) sen->SetValue(k, 0.5);
                sen->Pack();
                child = sen;
                break;
            }
        }
        children.push_back(child);
        msg->AppendChildMessage(child);
    }
    msg->Pack();
    auto* p = static_cast<const std::uint8_t*>(msg->GetBufferPointer());
    return {p, p + msg->GetBufferSize()};
}

// POLYDATA — a small mesh with points and polygons. Upstream's
// section-size bookkeeping is tricky; restrict to the simple case
// of points+polygons only (no lines, vertices, triangle strips, or
// attributes) to keep the vector small and deterministic.
std::vector<std::uint8_t> gen_polydata(std::mt19937_64& rng) {
    auto msg = igtl::PolyDataMessage::New();
    apply_common(msg, rng);
    msg->SetHeaderVersion(IGTL_HEADER_VERSION_1);
    auto pts = igtl::PolyDataPointArray::New();
    auto polys = igtl::PolyDataCellArray::New();
    std::uniform_int_distribution<int> npts_dist(3, 8);
    const int npts = npts_dist(rng);
    std::uniform_real_distribution<float> f(-10.0f, 10.0f);
    for (int i = 0; i < npts; ++i) {
        float pt[3] = {f(rng), f(rng), f(rng)};
        pts->AddPoint(pt);
    }
    // One triangle fan cell so polygons array has exactly one entry
    // of length npts.
    std::list<igtlUint32> cell;
    for (int i = 0; i < npts; ++i) cell.push_back(static_cast<igtlUint32>(i));
    polys->AddCell(cell);
    msg->SetPoints(pts);
    msg->SetPolygons(polys);
    msg->Pack();
    auto* p = static_cast<const std::uint8_t*>(msg->GetBufferPointer());
    return {p, p + msg->GetBufferSize()};
}

// TDATA — 1-3 tracking-data elements, each with a transform matrix.
std::vector<std::uint8_t> gen_tdata(std::mt19937_64& rng) {
    auto msg = igtl::TrackingDataMessage::New();
    apply_common(msg, rng);
    std::uniform_int_distribution<int> n(1, 3);
    std::uniform_real_distribution<float> f(-100.0f, 100.0f);
    for (int i = 0, m = n(rng); i < m; ++i) {
        auto e = igtl::TrackingDataElement::New();
        e->SetName(random_ascii_name(rng, 15).c_str());
        e->SetType(igtl::TrackingDataElement::TYPE_TRACKER);
        igtl::Matrix4x4 mat;
        igtl::IdentityMatrix(mat);
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c) mat[r][c] = f(rng);
        e->SetMatrix(mat);
        msg->AddTrackingDataElement(e);
    }
    msg->Pack();
    auto* p = static_cast<const std::uint8_t*>(msg->GetBufferPointer());
    return {p, p + msg->GetBufferSize()};
}

// QTDATA — 1-3 quaternion-tracking-data elements.
std::vector<std::uint8_t> gen_qtdata(std::mt19937_64& rng) {
    auto msg = igtl::QuaternionTrackingDataMessage::New();
    apply_common(msg, rng);
    std::uniform_int_distribution<int> n(1, 3);
    std::uniform_real_distribution<float> f(-1.0f, 1.0f);
    for (int i = 0, m = n(rng); i < m; ++i) {
        auto e = igtl::QuaternionTrackingDataElement::New();
        e->SetName(random_ascii_name(rng, 15).c_str());
        e->SetType(igtl::QuaternionTrackingDataElement::TYPE_TRACKER);
        e->SetPosition(f(rng) * 100, f(rng) * 100, f(rng) * 100);
        e->SetQuaternion(f(rng), f(rng), f(rng), f(rng));
        msg->AddQuaternionTrackingDataElement(e);
    }
    msg->Pack();
    auto* p = static_cast<const std::uint8_t*>(msg->GetBufferPointer());
    return {p, p + msg->GetBufferSize()};
}

// TRAJ — 1-3 trajectory elements, each with entry+target positions.
std::vector<std::uint8_t> gen_traj(std::mt19937_64& rng) {
    auto msg = igtl::TrajectoryMessage::New();
    apply_common(msg, rng);
    std::uniform_int_distribution<int> n(1, 3);
    std::uniform_real_distribution<float> f(-100.0f, 100.0f);
    std::uniform_int_distribution<int> byte(0, 255);
    for (int i = 0, m = n(rng); i < m; ++i) {
        auto e = igtl::TrajectoryElement::New();
        e->SetName(random_ascii_name(rng, 15).c_str());
        e->SetGroupName(random_ascii_name(rng, 15).c_str());
        e->SetType(igtl::TrajectoryElement::TYPE_ENTRY_TARGET);
        e->SetRGBA(
            static_cast<igtlUint8>(byte(rng)),
            static_cast<igtlUint8>(byte(rng)),
            static_cast<igtlUint8>(byte(rng)),
            static_cast<igtlUint8>(byte(rng)));
        e->SetEntryPosition(f(rng), f(rng), f(rng));
        e->SetTargetPosition(f(rng), f(rng), f(rng));
        e->SetRadius(static_cast<float>(std::abs(f(rng))));
        e->SetOwner(random_ascii_name(rng, 15).c_str());
        msg->AddTrajectoryElement(e);
    }
    msg->Pack();
    auto* p = static_cast<const std::uint8_t*>(msg->GetBufferPointer());
    return {p, p + msg->GetBufferSize()};
}

// CAPABILITY — a list of supported type_id strings.
std::vector<std::uint8_t> gen_capability(std::mt19937_64& rng) {
    auto msg = igtl::CapabilityMessage::New();
    apply_common(msg, rng);
    std::uniform_int_distribution<int> n(1, 6);
    std::vector<std::string> types;
    const int m = n(rng);
    types.reserve(m);
    for (int i = 0; i < m; ++i) {
        types.push_back(random_ascii_name(rng, 11));
    }
    msg->SetTypes(types);
    msg->Pack();
    auto* p = static_cast<const std::uint8_t*>(msg->GetBufferPointer());
    return {p, p + msg->GetBufferSize()};
}

// Round-robin dispatch. Ordering is stable across seeds so a given
// (seed, iteration) maps to a deterministic type.
using Generator = std::vector<std::uint8_t>(*)(std::mt19937_64&);
struct NamedGenerator {
    const char* type_id;
    Generator fn;
};
const NamedGenerator kGenerators[] = {
    {"TRANSFORM",  gen_transform},
    {"POSITION",   gen_position},
    {"STATUS",     gen_status},
    {"STRING",     gen_string},
    {"SENSOR",     gen_sensor},
    {"POINT",      gen_point},
    {"IMAGE",      gen_image},
    {"NDARRAY",    gen_ndarray},
    {"COLORT",     gen_colort},
    {"BIND",       gen_bind},
    {"POLYDATA",   gen_polydata},
    {"TDATA",      gen_tdata},
    {"QTDATA",     gen_qtdata},
    {"TRAJ",       gen_traj},
    {"CAPABILITY", gen_capability},
};

}  // namespace

int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::size_t count = 1000;
    std::uint64_t seed = 1;
    const char* restrict_type = nullptr;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--count" && i + 1 < argc) count = std::strtoull(argv[++i], nullptr, 10);
        else if (a == "--seed" && i + 1 < argc) seed = std::strtoull(argv[++i], nullptr, 10);
        else if (a == "--type" && i + 1 < argc) restrict_type = argv[++i];
        else {
            std::cerr << "unknown argument: " << a << "\n";
            return 2;
        }
    }
    std::mt19937_64 rng(seed);
    const std::size_t nGen = sizeof(kGenerators) / sizeof(kGenerators[0]);
    for (std::size_t i = 0; i < count; ++i) {
        Generator fn = nullptr;
        if (restrict_type != nullptr) {
            for (std::size_t j = 0; j < nGen; ++j) {
                if (std::strcmp(kGenerators[j].type_id, restrict_type) == 0) {
                    fn = kGenerators[j].fn;
                    break;
                }
            }
            if (fn == nullptr) {
                std::cerr << "unknown --type: " << restrict_type << "\n";
                return 2;
            }
        } else {
            fn = kGenerators[i % nGen].fn;
        }
        auto bytes = fn(rng);
        if (bytes.empty()) continue;
        emit_hex(bytes.data(), bytes.size());
    }
    std::cout.flush();
    return 0;
}
