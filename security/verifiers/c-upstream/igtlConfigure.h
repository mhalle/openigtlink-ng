/* igtlConfigure.h stand-in for building upstream's igtlutil C
 * source files without running upstream's CMake configure step.
 *
 * Upstream's build system generates igtlConfigure.h from
 * igtlConfigure.h.in using CMake. Upstream's C layer — the
 * igtlutil/ directory we're compiling here as a differential
 * fuzzer oracle — includes this header but only reads a small
 * number of macros from it:
 *
 *   - OpenIGTLink_PROTOCOL_VERSION_1/_2/_3  (constants 1/2/3)
 *   - OpenIGTLink_HEADER_VERSION            (used in #if guards
 *                                            in .c files)
 *
 * Platform defines (OpenIGTLink_PLATFORM_LINUX etc.), thread
 * choices (USE_PTHREADS etc.), and feature gates (USE_H264 etc.)
 * are only consumed by upstream's C++ layer — not by igtlutil —
 * so we omit them here.
 *
 * If a future upstream commit adds new config-gated code inside
 * igtlutil, this header will need to grow. The compile will
 * fail loudly if so, which is the signal to update.
 */
#ifndef __IGTL_CONFIGURE_H
#define __IGTL_CONFIGURE_H

#define OpenIGTLink_PROTOCOL_VERSION_1 1
#define OpenIGTLink_PROTOCOL_VERSION_2 2
#define OpenIGTLink_PROTOCOL_VERSION_3 3

/* Protocol / header version — the values upstream's CMake picks
 * for a v3-protocol build. Gates the `#if OpenIGTLink_HEADER_VERSION
 * >= 2` blocks inside igtl_command.c etc. */
#define OpenIGTLink_PROTOCOL_VERSION   3
#define OpenIGTLink_HEADER_VERSION     2

#endif /* __IGTL_CONFIGURE_H */
