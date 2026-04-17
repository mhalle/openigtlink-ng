// igtlOSUtil.h — upstream's OS helper. Sleep() blocks the calling
// thread for a millisecond count; Strnlen bounds a string length.
//
// Upstream examples include this liberally (all the Server.cxx /
// Client.cxx programs use `igtl::Sleep(interval)`). Thin wrappers
// over <thread> / <cstring> — no platform forks needed.
#ifndef __igtlOSUtil_h
#define __igtlOSUtil_h

#include <chrono>
#include <cstddef>
#include <cstring>
#include <thread>

#include "igtlMacro.h"

namespace igtl {

/// Sleep the current thread for `millisecond` milliseconds.
inline void IGTLCommon_EXPORT Sleep(int millisecond) {
    if (millisecond <= 0) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(millisecond));
}

/// Bounded strlen — never reads past `maxlen` bytes.
inline std::size_t IGTLCommon_EXPORT Strnlen(const char* s, std::size_t maxlen) {
#if defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
    return ::strnlen(s, maxlen);
#else
    std::size_t i = 0;
    while (i < maxlen && s[i] != '\0') ++i;
    return i;
#endif
}

}  // namespace igtl

#endif  // __igtlOSUtil_h
