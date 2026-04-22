// Plus-surface header sanity — the shim covers the entire igtl::
// API surface PLUS Toolkit touches.
//
// The test pulls in every header PLUS's PlusOpenIGTLink + PlusServer
// modules #include with an "igtl" prefix, then references every
// igtl:: symbol the audit of PLUS's source turned up. The goal is
// compile-time proof that a PLUS tree switched from upstream
// OpenIGTLink to our shim would resolve every symbol we're
// responsible for.
//
// Excluded on purpose: igtlio* (separate library), PLUS-internal
// classes (vtkPlus*, PlusCommon.h), VTK core (vtkObject, etc.),
// IGSIO (igsioTrackedFrame, vtkIGSIO*). Those are upstream-of-us
// dependencies PLUS carries regardless of which IGTL it links.
// Phase 3b would pin them — that's out of this test's scope.
//
// Compile-only: `main` runs but does no real work. A green build
// is the pass signal; CTest exit status 0 confirms nothing linked
// wrong either.

// ---- C-level headers PLUS includes by underscore name.
#include "igtl/igtl_header.h"
#include "igtl/igtl_util.h"
#include "igtl/igtl_image.h"
#include "igtl/igtl_tdata.h"
#include "igtl/igtl_types.h"
#include "igtl/igtl_video.h"

// ---- C++ shim headers — every one PLUS #includes.
#include "igtl/igtlClientSocket.h"
#include "igtl/igtlCommandMessage.h"
#include "igtl/igtlCommon.h"
#include "igtl/igtlGetImageMessage.h"
#include "igtl/igtlGetImageMetaMessage.h"
#include "igtl/igtlGetPointMessage.h"
#include "igtl/igtlGetPolyDataMessage.h"
#include "igtl/igtlGetStatusMessage.h"
#include "igtl/igtlImageMessage.h"
#include "igtl/igtlImageMetaMessage.h"
#include "igtl/igtlMath.h"
#include "igtl/igtlMessageBase.h"
#include "igtl/igtlMessageFactory.h"
#include "igtl/igtlMessageHeader.h"
#include "igtl/igtlOSUtil.h"
#include "igtl/igtlPointMessage.h"
#include "igtl/igtlPolyDataMessage.h"
#include "igtl/igtlPositionMessage.h"
#include "igtl/igtlRTSTrackingDataMessage.h"
#include "igtl/igtlServerSocket.h"
#include "igtl/igtlSocket.h"
#include "igtl/igtlStartTrackingDataMessage.h"
#include "igtl/igtlStatusMessage.h"
#include "igtl/igtlStopTrackingDataMessage.h"
#include "igtl/igtlStringMessage.h"
#include "igtl/igtlTimeStamp.h"
#include "igtl/igtlTrackingDataMessage.h"
#include "igtl/igtlTransformMessage.h"

#include <cstdio>

// Each block declares a pointer/Pointer to force class-level
// resolution. Unused — we want compile-time coverage, not runtime.
static void use_message_types() {
    igtl::MessageBase::Pointer       base;
    igtl::MessageHeader::Pointer     hdr;
    igtl::MessageFactory::Pointer    fac;
    igtl::TimeStamp::Pointer         ts;
    igtl::TransformMessage::Pointer  tx;
    igtl::PositionMessage::Pointer   pos;
    igtl::StatusMessage::Pointer     st;
    igtl::GetStatusMessage::Pointer  gst;
    igtl::StringMessage::Pointer     str;
    igtl::CommandMessage::Pointer    cmd;
    igtl::RTSCommandMessage::Pointer rcmd;
    igtl::ImageMessage::Pointer      img;
    igtl::GetImageMessage::Pointer   gimg;
    igtl::ImageMetaMessage::Pointer  imeta;
    igtl::ImageMetaElement::Pointer  imetaEl;
    igtl::GetImageMetaMessage::Pointer gimeta;
    igtl::PointMessage::Pointer      pt;
    igtl::PointElement::Pointer      ptEl;
    igtl::GetPointMessage::Pointer   gpt;
    igtl::PolyDataMessage::Pointer   poly;
    igtl::GetPolyDataMessage::Pointer gpoly;
    igtl::RTSPolyDataMessage::Pointer rpoly;
    igtl::TrackingDataMessage::Pointer tdata;
    igtl::TrackingDataElement::Pointer tdataEl;
    igtl::StartTrackingDataMessage::Pointer stt;
    igtl::StopTrackingDataMessage::Pointer  stp;
    igtl::RTSTrackingDataMessage::Pointer   rts;

    igtl::ClientSocket::Pointer cs;
    igtl::ServerSocket::Pointer ss;
    igtl::Socket::Pointer       sock;

    (void)base; (void)hdr; (void)fac; (void)ts; (void)tx; (void)pos;
    (void)st; (void)gst; (void)str; (void)cmd; (void)rcmd; (void)img;
    (void)gimg; (void)imeta; (void)imetaEl; (void)gimeta; (void)pt;
    (void)ptEl; (void)gpt; (void)poly; (void)gpoly; (void)rpoly;
    (void)tdata; (void)tdataEl; (void)stt; (void)stp; (void)rts;
    (void)cs; (void)ss; (void)sock;
}

static void use_free_functions() {
    // igtl::Math — matrix/quaternion helpers PLUS uses.
    igtl::Matrix4x4 m;
    igtl::IdentityMatrix(m);
    float q[4];
    igtl::MatrixToQuaternion(m, q);
    igtl::QuaternionToMatrix(q, m);

    // Protocol/header version lookup.
    const int hv = igtl::IGTLProtocolToHeaderLookup(
        OpenIGTLink_PROTOCOL_VERSION_3);
    (void)hv;

    // C-level macros — constants PLUS writes into messages.
    static_assert(IGTL_HEADER_VERSION_1 == 1, "header v1 mismatch");
    static_assert(IGTL_HEADER_VERSION_2 == 2, "header v2 mismatch");
    static_assert(IGTL_HEADER_SIZE == 58,     "header size mismatch");
    static_assert(IGTL_IMAGE_HEADER_SIZE == 72, "image hdr size");
    static_assert(IGTL_TDATA_LEN_NAME == 20,  "tdata name len");
    static_assert(IGTL_VIDEO_ENDIAN_BIG == 1, "video endian big");
    static_assert(IGTL_VIDEO_ENDIAN_LITTLE == 2, "video endian lil");

    // Runtime-probed endian helper.
    (void)igtl_is_little_endian();
}

int main() {
    use_message_types();
    use_free_functions();
    std::fprintf(stderr, "plus_header_surface: all OK\n");
    return 0;
}
