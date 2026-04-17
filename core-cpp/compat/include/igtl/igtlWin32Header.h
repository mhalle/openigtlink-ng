// igtlWin32Header.h — upstream's cross-platform macro header.
//
// Upstream uses this to stamp __declspec(dllexport) / dllimport on
// Windows DLL builds. We target static-only, so the macros are
// empty. This file exists solely so consumers that
// `#include "igtlWin32Header.h"` (directly or transitively) find
// something.
#ifndef __igtlWin32Header_h
#define __igtlWin32Header_h

// Already defined by igtlMacro.h (we include it here so any TU
// including only igtlWin32Header.h still sees the macros).
#include "igtlMacro.h"

#endif  // __igtlWin32Header_h
