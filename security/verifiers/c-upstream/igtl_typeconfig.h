/* igtl_typeconfig.h stand-in — companion to the sibling
 * igtlConfigure.h in this directory. Upstream generates this at
 * CMake-configure time; we supply a minimal version hand-tuned
 * for every platform we run CI on (x86_64 / ARM64, Linux / macOS /
 * Windows). Sizes are the same on all of them.
 *
 * Upstream's igtl_types.h reads these macros to pick the
 * fixed-width int typedefs. If we ever care about a 32-bit-long
 * target here, this file will need a platform branch — LP64 vs
 * ILP32 diverges on long only. For now, all supported targets
 * have int=4 / long=8 / long long=8, which is what these values
 * express.
 */
#ifndef __IGTL_TYPECONFIG_H
#define __IGTL_TYPECONFIG_H

#define IGTL_SIZEOF_CHAR    1
#define IGTL_SIZEOF_SHORT   2
#define IGTL_SIZEOF_INT     4
#define IGTL_SIZEOF_LONG    8
#define IGTL_SIZEOF_FLOAT   4
#define IGTL_SIZEOF_DOUBLE  8

/* Pick long long for the 64-bit path — matches upstream's CMake
 * logic on every modern 64-bit target. */
#define IGTL_TYPE_USE_LONG_LONG 1
#define IGTL_SIZEOF_LONG_LONG   8

#endif  /* __IGTL_TYPECONFIG_H */
