// libFuzzer entry point for the upstream-compat shim's hand-written
// UnpackContent() parsers.
//
// WHY A SEPARATE FUZZER?
// -----------------------
// fuzz_oracle drives the codegen'd codecs in oigtl_messages (through
// verify_wire_bytes). The shim layer under compat/ has its own
// UnpackContent() overrides — hand-written parsers that do NOT share
// code with the codegen path. A bug in compat/src/igtlPolyDataMessage.cxx
// (for example) is invisible to fuzz_oracle.
//
// This entry point synthesises a valid outer 58-byte header around
// arbitrary body bytes, so the fuzzer spends cycles inside the body
// parser rather than learning the header invariants. The first byte
// of the fuzz input is a selector: it picks which facade to drive.
//
// SELECTOR TABLE — ordered by risk assessment (counted sub-arrays,
// nested structures, dim-product arithmetic):
//
//    0  PolyDataMessage        points/verts/cells + counted attributes
//    1  BindMessage            N child messages with size prefixes
//    2  NDArrayMessage         rank + dim sizes + typed payload
//    3  ImageMetaMessage       count × 260-byte records
//    4  LabelMetaMessage       count × 116-byte records
//    5  TrajectoryMessage      count × 150-byte records
//    6  PointMessage           count × 136-byte records
//    7  TrackingDataMessage    count × 70-byte records
//    8  QuaternionTrackingDataMessage  count × 50-byte records
//    9  ImageMessage           72-byte header + pixel payload
//   10  ColorTableMessage      header + typed table
//   11  CommandMessage         length-prefixed strings
//   12  SensorMessage          length + typed array
//   13  StringMessage          length-prefixed UTF-8 (easy baseline)
//
// Any ASan / UBSan trap or uncaught exception escaping this harness
// is a real bug. Upstream API contract: Unpack(0) should return a
// status flag or leave the message in an indeterminate-but-safe
// state — never crash.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "igtl/igtlBindMessage.h"
#include "igtl/igtlCapabilityMessage.h"
#include "igtl/igtlColorTableMessage.h"
#include "igtl/igtlCommandMessage.h"
#include "igtl/igtlImageMessage.h"
#include "igtl/igtlImageMetaMessage.h"
#include "igtl/igtlLabelMetaMessage.h"
#include "igtl/igtlMessageBase.h"
#include "igtl/igtlMessageHeader.h"
#include "igtl/igtlNDArrayMessage.h"
#include "igtl/igtlPointMessage.h"
#include "igtl/igtlPolyDataMessage.h"
#include "igtl/igtlQuaternionTrackingDataMessage.h"
#include "igtl/igtlSensorMessage.h"
#include "igtl/igtlStringMessage.h"
#include "igtl/igtlTrackingDataMessage.h"
#include "igtl/igtlTrajectoryMessage.h"

#include "oigtl/runtime/header.hpp"

namespace {

// Synthesise a wire buffer: valid 58-byte header (version=2,
// chosen type_id, body_size=body.size(), crc computed fresh) +
// the provided body bytes verbatim.
std::vector<std::uint8_t>
build_wire(const char* type_id,
           const std::uint8_t* body, std::size_t body_size) {
    std::vector<std::uint8_t> wire(
        oigtl::runtime::kHeaderSize + body_size);
    std::array<std::uint8_t, oigtl::runtime::kHeaderSize> hdr{};
    oigtl::runtime::pack_header(
        hdr,
        /*version=*/2,
        std::string(type_id),
        std::string("FuzzDev"),
        /*timestamp=*/0,
        body, body_size);
    std::memcpy(wire.data(), hdr.data(), hdr.size());
    if (body_size > 0) {
        std::memcpy(wire.data() + oigtl::runtime::kHeaderSize,
                    body, body_size);
    }
    return wire;
}

// Drive a facade: copy wire into the facade's internal buffer via
// the pointer-stable GetPackPointer() path, then Unpack().
template <class Msg>
void drive(const char* type_id,
           const std::uint8_t* body, std::size_t body_size) {
    auto msg = Msg::New();
    auto wire = build_wire(type_id, body, body_size);
    // Mirror what a real receiver does:
    //   header.InitPack();
    //   memcpy(header.GetPackPointer(), wire, 58);
    //   header.Unpack();   // header-only
    //   msg.SetMessageHeader(header);  msg.AllocatePack();
    //   memcpy(msg.GetPackBodyPointer(), wire+58, body_size);
    //   msg.Unpack(0);
    igtl::MessageHeader::Pointer hdr = igtl::MessageHeader::New();
    hdr->InitPack();
    std::memcpy(hdr->GetPackPointer(), wire.data(),
                oigtl::runtime::kHeaderSize);
    hdr->Unpack();  // no CRC — we computed it, but keep the path honest
    msg->SetMessageHeader(hdr);
    msg->AllocatePack();
    if (body_size > 0) {
        // AllocatePack sizes the buffer from the header's body_size.
        std::memcpy(msg->GetPackBodyPointer(),
                    wire.data() + oigtl::runtime::kHeaderSize,
                    body_size);
    }
    try {
        // crccheck=0: we control the wire; CRC arithmetic isn't
        // what we're fuzzing here. Focus fire on UnpackContent.
        (void)msg->Unpack(0);
    } catch (...) {
        // An uncaught exception escaping Unpack is a shim bug —
        // Unpack is contractually silent (returns status bits).
        // Rethrow so libFuzzer records it as a crash.
        throw;
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size) {
    if (size < 1) return 0;
    const std::uint8_t selector = data[0];
    const std::uint8_t* body = data + 1;
    const std::size_t body_size = size - 1;

    switch (selector % 14) {
        case 0:
            drive<igtl::PolyDataMessage>("POLYDATA", body, body_size);
            break;
        case 1:
            drive<igtl::BindMessage>("BIND", body, body_size);
            break;
        case 2:
            drive<igtl::NDArrayMessage>("NDARRAY", body, body_size);
            break;
        case 3:
            drive<igtl::ImageMetaMessage>("IMGMETA", body, body_size);
            break;
        case 4:
            drive<igtl::LabelMetaMessage>("LBMETA", body, body_size);
            break;
        case 5:
            drive<igtl::TrajectoryMessage>("TRAJ", body, body_size);
            break;
        case 6:
            drive<igtl::PointMessage>("POINT", body, body_size);
            break;
        case 7:
            drive<igtl::TrackingDataMessage>("TDATA", body, body_size);
            break;
        case 8:
            drive<igtl::QuaternionTrackingDataMessage>(
                "QTDATA", body, body_size);
            break;
        case 9:
            drive<igtl::ImageMessage>("IMAGE", body, body_size);
            break;
        case 10:
            drive<igtl::ColorTableMessage>("COLORT", body, body_size);
            break;
        case 11:
            drive<igtl::CommandMessage>("COMMAND", body, body_size);
            break;
        case 12:
            drive<igtl::SensorMessage>("SENSOR", body, body_size);
            break;
        case 13:
            drive<igtl::StringMessage>("STRING", body, body_size);
            break;
    }
    return 0;
}
