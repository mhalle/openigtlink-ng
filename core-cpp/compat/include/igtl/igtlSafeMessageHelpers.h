// igtlSafeMessageHelpers.h
//
// Marker / declaration header for the openigtlink-ng compat shim's
// sanctioned tier-2 protected API on igtl::MessageBase. Exists for
// two reasons:
//
// 1. **As an `__has_include` detection target.** Downstream code
//    that wants to support both this shim and upstream OpenIGTLink
//    can write
//
//        #if defined(__has_include) && \
//            __has_include(<igtl/igtlSafeMessageHelpers.h>)
//          // Our shim is on the include path; the safe API exists.
//        #endif
//
//    The header file itself doesn't need to do much — its presence
//    is the signal. A consumer that writes the above pattern need
//    not depend on a build-system-provided macro like
//    `OIGTL_NG_SHIM`. (`OIGTL_NG_SHIM` is still available via
//    `igtlMacro.h` for callers that prefer the macro idiom; the
//    feature-test header is the upstream-friendly form.)
//
// 2. **As declarative documentation.** This header re-declares the
//    tier-2 API methods as comments below so a reader can find the
//    safe extension surface in one place without grepping
//    `igtlMessageBase.h`.
//
// Tier-2 API summary (defined in igtl/igtlMessageBase.h):
//
//   protected:
//     std::uint8_t*       GetContentPointer();
//     const std::uint8_t* GetContentPointer() const;
//     std::size_t         GetContentSize() const;
//     int                 CopyReceivedFrom(const MessageBase& src);
//
// These exist *in addition to* upstream's protected members
// (`m_Content`, `m_MessageSize`, `m_MetaDataMap`, `CopyHeader`,
// `CopyBody`, `InitBuffer`, `AllocateBuffer`). Subclasses that
// reach into upstream's protected internals continue to work —
// the new methods are an opt-in safer path, not a replacement.

#ifndef IGTL_SAFE_MESSAGE_HELPERS_H_
#define IGTL_SAFE_MESSAGE_HELPERS_H_

// Forward-include of MessageBase so consumers who only #include
// this header still see the API they're feature-testing.
#include "igtlMessageBase.h"

// Versioning marker — a downstream feature-test can probe this if
// it cares about which sanctioned-API revision is available.
//
//   1: initial (CopyReceivedFrom + GetContentPointer family).
#define IGTL_SAFE_MESSAGE_HELPERS_VERSION 1

#endif  // IGTL_SAFE_MESSAGE_HELPERS_H_
