// igtlMacro.h — upstream-compatible macros for the shim.
//
// This header matches the public API that consumers of upstream
// (Slicer, PLUS, etc.) rely on. The implementations are ours:
// `igtlNewMacro` goes through our `ObjectFactory<T>::Create` stub
// which always returns null, falling through to `new x` — matching
// upstream's default (no registered plugin factories) behaviour.
//
// Subset for Phase 5. Accessor macros (`igtlSetMacro`, `igtlGetMacro`,
// `igtlSetStringMacro`, `igtlGetStringMacro`) are included because
// upstream's generated message headers use them extensively; we want
// consumers to compile unchanged in Phase 6.
#ifndef __igtlMacro_h
#define __igtlMacro_h

#include <string>

// Feature-test macro — downstream consumers (e.g., a PLUS fork
// with compatibility ifdefs) can branch on `defined(OIGTL_NG_SHIM)`
// to target the sanctioned tier-2 protected API (GetContentPointer,
// CopyReceivedFrom, etc.) when compiled against us, while keeping
// the upstream-idiom path for builds against the reference
// OpenIGTLink library. See core-cpp/compat/API_COVERAGE.md for the
// full contract.
#ifndef OIGTL_NG_SHIM
#define OIGTL_NG_SHIM 1
#endif

// Export decoration for DLL boundaries. Empty on static / non-Windows
// builds — which is our target. Defined for source compatibility with
// upstream's public headers.
#ifndef IGTLCommon_EXPORT
#define IGTLCommon_EXPORT
#endif
#ifndef IGTL_EXPORT
#define IGTL_EXPORT
#endif
#ifndef OpenIGTLink_EXPORT
#define OpenIGTLink_EXPORT
#endif

// Debug / warning macros used by upstream headers. No-op in our
// shim; consumers can route them to their own logging by
// predefining these before including igtl headers.
#ifndef igtlDebugMacro
#define igtlDebugMacro(x) do { } while (0)
#endif
#ifndef igtlWarningMacro
#define igtlWarningMacro(x) do { } while (0)
#endif
#ifndef igtlErrorMacro
#define igtlErrorMacro(x) do { } while (0)
#endif

// Forward-declare the factory stub so igtlNewMacro can use it
// without each header having to #include igtlObjectFactory.h.
namespace igtl {
template <class T> class ObjectFactory;
}

// All macros end with `static_assert(true, "")` (no trailing `;`)
// so that call sites like `igtlTypeMacro(Foo, Bar);` expand to
// well-formed `static_assert(…);`. Without this, the caller's
// trailing semicolon becomes a stray semi, which g++ flags as
// `-Werror=pedantic` ("extra ';'") inside class bodies. Upstream's
// headers have the same issue but upstream's CMake doesn't use
// `-Wpedantic`. The static_assert idiom makes the call-site
// `;` required and warning-free under all our compilers.
#define OIGTL_REQUIRE_SEMICOLON static_assert(true, "")

/** Type-traits macro: declares Self, Superclass, Pointer,
    ConstPointer typedefs and overrides GetNameOfClass(). */
#define igtlTypeMacro(thisClass, superclass) \
    const char* GetNameOfClass() const override { return #thisClass; } \
    typedef thisClass                    Self; \
    typedef superclass                   Superclass; \
    typedef ::igtl::SmartPointer<Self>        Pointer; \
    typedef ::igtl::SmartPointer<const Self>  ConstPointer; \
    OIGTL_REQUIRE_SEMICOLON

/** Standard factory creation: produces a `static Pointer New()` that
    goes through `ObjectFactory<x>::Create` (null by default) and
    falls back to `new x`. Matches upstream behaviour. */
#define igtlNewMacro(x) \
    static Pointer New(void) { \
        Pointer smartPtr = ::igtl::ObjectFactory<x>::Create(); \
        if (smartPtr.GetPointer() == nullptr) { \
            smartPtr = new x; \
        } \
        smartPtr->UnRegister(); \
        return smartPtr; \
    } \
    ::igtl::LightObject::Pointer CreateAnother(void) const override { \
        return x::New().GetPointer(); \
    } \
    OIGTL_REQUIRE_SEMICOLON

/** Setter/getter macros widely used in upstream-shaped headers.
    They're strictly ABI-stable renames of field access. */
#define igtlSetMacro(name, type) \
    virtual void Set##name(const type _arg) { \
        if (this->m_##name != _arg) { this->m_##name = _arg; } \
    } OIGTL_REQUIRE_SEMICOLON
#define igtlGetMacro(name, type) \
    virtual type Get##name() { return this->m_##name; } \
    OIGTL_REQUIRE_SEMICOLON
#define igtlGetConstMacro(name, type) \
    virtual type Get##name() const { return this->m_##name; } \
    OIGTL_REQUIRE_SEMICOLON

#define igtlSetStringMacro(name) \
    virtual void Set##name(const char* _arg) { \
        if (_arg == nullptr) { this->m_##name.clear(); return; } \
        this->m_##name = _arg; \
    } OIGTL_REQUIRE_SEMICOLON
#define igtlGetStringMacro(name) \
    virtual const char* Get##name() const { \
        return this->m_##name.c_str(); \
    } OIGTL_REQUIRE_SEMICOLON

#endif  // __igtlMacro_h
