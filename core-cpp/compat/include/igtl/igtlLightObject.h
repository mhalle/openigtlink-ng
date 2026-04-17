// igtlLightObject.h — base class for every upstream-API object.
//
// Public surface matches upstream's header exactly: Self / Pointer /
// ConstPointer typedefs, New(), CreateAnother(), Delete(), Print(),
// Register() / UnRegister() / GetReferenceCount() / SetReferenceCount().
//
// Implementation departures:
//   - Ref count is `mutable std::atomic<int>`, not
//     `volatile int + SimpleFastMutexLock`. Lock-free,
//     same semantics, smaller binary.
//   - `SetReferenceCount(0)` still deletes (matches upstream).
//
// Ownership model (identical to upstream):
//   new Foo         → refcount = 1  (the "birth" reference)
//   Pointer p(raw)  → refcount = 2  (SmartPointer registered)
//   p.UnRegister()-> refcount = 1  (igtlNewMacro does this once)
//   p out of scope  → refcount = 0  → delete this
#ifndef __igtlLightObject_h
#define __igtlLightObject_h

#include "igtlMacro.h"
#include "igtlObjectFactory.h"
#include "igtlSmartPointer.h"

#include <atomic>
#include <iostream>
#include <typeinfo>

namespace igtl {

class IGTLCommon_EXPORT LightObject {
 public:
    typedef LightObject         Self;
    typedef SmartPointer<Self>        Pointer;
    typedef SmartPointer<const Self>  ConstPointer;

    // Factory entry. Plain `new LightObject` since there's no
    // upstream factory registration here. Subclasses override via
    // `igtlNewMacro(ThisClass)`.
    static Pointer New();

    virtual Pointer CreateAnother() const;

    // Dereferences one "birth" refcount, triggering destruction if
    // that was the last. Upstream consumers call `obj->Delete()`
    // rather than `delete obj` because the C++ delete bypasses the
    // refcount.
    virtual void Delete();

    virtual const char* GetNameOfClass() const { return "LightObject"; }

    void Print(std::ostream& os) const;

    static void BreakOnError();

    virtual void Register()   const;
    virtual void UnRegister() const;

    virtual int  GetReferenceCount() const {
        return m_ReferenceCount.load(std::memory_order_acquire);
    }

    // Dangerous — matches upstream. Setting to 0 triggers delete.
    virtual void SetReferenceCount(int n);

 protected:
    LightObject() : m_ReferenceCount(1) {}
    virtual ~LightObject();

    virtual void PrintSelf(std::ostream& os) const;
    virtual void PrintHeader(std::ostream& os) const;
    virtual void PrintTrailer(std::ostream& os) const;

    mutable std::atomic<int> m_ReferenceCount;

 private:
    LightObject(const Self&)            = delete;
    void operator=(const Self&)         = delete;
};

}  // namespace igtl

#endif  // __igtlLightObject_h
