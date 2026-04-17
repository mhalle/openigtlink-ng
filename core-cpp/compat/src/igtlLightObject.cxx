// igtlLightObject.cxx — thread-safe intrusive refcount on
// std::atomic<int>.

#include "igtl/igtlLightObject.h"
#include "igtl/igtlObjectFactory.h"

#include <iostream>
#include <typeinfo>

namespace igtl {

LightObject::Pointer LightObject::New() {
    // Consistent with `igtlNewMacro`: try the factory (stub returns
    // null), fall back to `new`, then UnRegister to drop the
    // birth reference so the returned Pointer owns exactly 1.
    Pointer sp = ObjectFactory<LightObject>::Create();
    if (sp.GetPointer() == nullptr) {
        sp = new LightObject;
    }
    sp->UnRegister();
    return sp;
}

LightObject::Pointer LightObject::CreateAnother() const {
    return LightObject::New();
}

void LightObject::Delete() {
    this->UnRegister();
}

void LightObject::Register() const {
    m_ReferenceCount.fetch_add(1, std::memory_order_relaxed);
}

void LightObject::UnRegister() const {
    // fetch_sub returns the previous value; `1` means we're the
    // last reference and must delete. memory_order_acq_rel on
    // decrement pairs with the acquire barrier below so non-atomic
    // member state written prior to the final UnRegister is
    // visible to the deleter thread.
    const int prev = m_ReferenceCount.fetch_sub(
        1, std::memory_order_acq_rel);
    if (prev == 1) {
        // Acquire barrier not strictly needed on x86/ARM64 after
        // the acq_rel above, but makes the intent explicit.
        std::atomic_thread_fence(std::memory_order_acquire);
        delete this;
    }
}

void LightObject::SetReferenceCount(int n) {
    m_ReferenceCount.store(n, std::memory_order_release);
    if (n == 0) {
        std::atomic_thread_fence(std::memory_order_acquire);
        delete this;
    }
}

LightObject::~LightObject() {
    // Upstream asserts a nonzero refcount here under debug builds;
    // we just rely on the invariant and let testing catch bugs.
}

void LightObject::Print(std::ostream& os) const {
    this->PrintHeader(os);
    this->PrintSelf(os);
    this->PrintTrailer(os);
}

void LightObject::PrintSelf(std::ostream& os) const {
    os << "RefCount: "
       << m_ReferenceCount.load(std::memory_order_acquire) << "\n";
}

void LightObject::PrintHeader(std::ostream& os) const {
    os << typeid(*this).name() << " (" << this << ")\n";
}

void LightObject::PrintTrailer(std::ostream& os) const {
    os << std::endl;
}

void LightObject::BreakOnError() {
    // Upstream's hook for debugger breakpoints. No-op here.
}

}  // namespace igtl
