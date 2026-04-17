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

/** Type-traits macro: declares Self, Superclass, Pointer,
    ConstPointer typedefs and overrides GetNameOfClass(). */
#define igtlTypeMacro(thisClass, superclass) \
    const char* GetNameOfClass() const override { return #thisClass; } \
    typedef thisClass                    Self; \
    typedef superclass                   Superclass; \
    typedef ::igtl::SmartPointer<Self>        Pointer; \
    typedef ::igtl::SmartPointer<const Self>  ConstPointer;

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
    }

/** Setter/getter macros widely used in upstream-shaped headers.
    They're strictly ABI-stable renames of field access. */
#define igtlSetMacro(name, type) \
    virtual void Set##name(const type _arg) { \
        if (this->m_##name != _arg) { this->m_##name = _arg; } \
    }
#define igtlGetMacro(name, type) \
    virtual type Get##name() { return this->m_##name; }
#define igtlGetConstMacro(name, type) \
    virtual type Get##name() const { return this->m_##name; }

#define igtlSetStringMacro(name) \
    virtual void Set##name(const char* _arg) { \
        if (_arg == nullptr) { this->m_##name.clear(); return; } \
        this->m_##name = _arg; \
    }
#define igtlGetStringMacro(name) \
    virtual const char* Get##name() const { \
        return this->m_##name.c_str(); \
    }

#endif  // __igtlMacro_h
