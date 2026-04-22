// igtlCommon.h — protocol-version helper shim.
//
// Upstream's `Source/igtlCommon.h` exposes `IGTLProtocolToHeaderLookup`
// as the canonical way to turn a declared protocol version (1/2/3) into
// the corresponding header-framing version (1 for v1+v2 protocol,
// 2 for v3 protocol). PLUS's `vtkPlusOpenIGTLinkClient` calls this
// helper on every send/receive to pick the correct framing.
//
// Keep it inline and side-effect-free so consumers of the shim
// don't need a separate .cxx compiled in.
#ifndef __igtlCommon_h
#define __igtlCommon_h

#include "igtl_header.h"

namespace igtl {

inline int IGTLProtocolToHeaderLookup(int igtlProtocolVersion) {
    if (igtlProtocolVersion == OpenIGTLink_PROTOCOL_VERSION_1 ||
        igtlProtocolVersion == OpenIGTLink_PROTOCOL_VERSION_2) {
        return IGTL_HEADER_VERSION_1;
    }
    if (igtlProtocolVersion == OpenIGTLink_PROTOCOL_VERSION_3) {
        return IGTL_HEADER_VERSION_2;
    }
    return IGTL_HEADER_VERSION_1;
}

}  // namespace igtl

#endif  // __igtlCommon_h
