/* igtl_header.h — C-level constants PLUS (and upstream consumers)
 * pull in alongside the C++ shim. Matches the values in upstream's
 * Source/igtlutil/igtl_header.h one-for-one. Kept minimal: only
 * what the audited PLUS consumer touches. Expand as Phase 3b builds
 * surface new references.
 */
#ifndef __igtl_header_h
#define __igtl_header_h

#ifdef __cplusplus
extern "C" {
#endif

#define IGTL_HEADER_VERSION_1   1
#define IGTL_HEADER_VERSION_2   2
#define IGTL_HEADER_SIZE        58

/* Protocol version identifiers — upstream puts these in
 * igtlConfigure.h (generated). Duplicated here so consumers don't
 * need to find-or-generate the configure header.
 */
#ifndef OpenIGTLink_PROTOCOL_VERSION_1
#define OpenIGTLink_PROTOCOL_VERSION_1 1
#endif
#ifndef OpenIGTLink_PROTOCOL_VERSION_2
#define OpenIGTLink_PROTOCOL_VERSION_2 2
#endif
#ifndef OpenIGTLink_PROTOCOL_VERSION_3
#define OpenIGTLink_PROTOCOL_VERSION_3 3
#endif

#ifdef __cplusplus
}
#endif

#endif  /* __igtl_header_h */
