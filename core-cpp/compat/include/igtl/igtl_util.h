/* igtl_util.h — the fraction of upstream's `Source/igtlutil/
 * igtl_util.h` that PLUS actually references. Right now that's
 * `igtl_is_little_endian()`; expand when Phase 3b surfaces more.
 *
 * The definition mirrors upstream's: return 1 on little-endian hosts,
 * 0 on big-endian. (Upstream returns `int`; PLUS code checks
 * `== 1` / `== true` interchangeably, which is fine because C's
 * implicit int↔bool conversion handles both.)
 */
#ifndef __igtl_util_h
#define __igtl_util_h

#ifdef __cplusplus
extern "C" {
#endif

static inline int igtl_is_little_endian(void) {
    const unsigned short probe = 0x0001;
    return *(const unsigned char*)(&probe) == 1 ? 1 : 0;
}

#ifdef __cplusplus
}
#endif

#endif  /* __igtl_util_h */
