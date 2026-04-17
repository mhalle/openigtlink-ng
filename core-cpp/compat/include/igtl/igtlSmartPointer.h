// igtlSmartPointer.h — intrusive reference-counted pointer, binary-
// compatible with upstream's SmartPointer<T> semantics.
//
// Design note: we match upstream's class shape exactly (same public
// members, same typedef `ObjectType`, same operator set). Pointed-to
// type must provide `Register()` and `UnRegister()` — satisfied by
// `LightObject` and anything that inherits it.
//
// NOT a std::shared_ptr. Upstream-compatible consumer code uses
// `Foo::Pointer` (= `SmartPointer<Foo>`) and expects `->` / `*` to
// forward to the pointee. An intrusive refcount is required because
// raw `Foo*` and `Foo::Pointer` must refer to the same control
// block — something std::shared_ptr can't give without enable_shared_
// from_this gymnastics that wouldn't be drop-in.
#ifndef __igtlSmartPointer_h
#define __igtlSmartPointer_h

#include "igtlMacro.h"

#include <iostream>

namespace igtl {

template <class TObjectType>
class IGTL_EXPORT SmartPointer {
 public:
    typedef TObjectType ObjectType;

    SmartPointer() : m_Pointer(nullptr) {}

    SmartPointer(const SmartPointer<ObjectType>& p)
        : m_Pointer(p.m_Pointer) { this->Register(); }

    SmartPointer(ObjectType* p) : m_Pointer(p) { this->Register(); }

    ~SmartPointer() {
        this->UnRegister();
        m_Pointer = nullptr;
    }

    ObjectType* operator->() const { return m_Pointer; }
    operator ObjectType*() const { return m_Pointer; }

    bool IsNotNull() const { return m_Pointer != nullptr; }
    bool IsNull()    const { return m_Pointer == nullptr; }

    template <typename R>
    bool operator==(R r) const {
        return m_Pointer == static_cast<const ObjectType*>(r);
    }
    template <typename R>
    bool operator!=(R r) const {
        return m_Pointer != static_cast<const ObjectType*>(r);
    }

    ObjectType* GetPointer() const { return m_Pointer; }

    bool operator<(const SmartPointer& r) const {
        return (void*)m_Pointer < (void*)r.m_Pointer;
    }
    bool operator>(const SmartPointer& r) const {
        return (void*)m_Pointer > (void*)r.m_Pointer;
    }
    bool operator<=(const SmartPointer& r) const {
        return (void*)m_Pointer <= (void*)r.m_Pointer;
    }
    bool operator>=(const SmartPointer& r) const {
        return (void*)m_Pointer >= (void*)r.m_Pointer;
    }

    SmartPointer& operator=(const SmartPointer& r) {
        return this->operator=(r.GetPointer());
    }

    SmartPointer& operator=(ObjectType* r) {
        if (m_Pointer != r) {
            // Retain current in a local so Register of the new
            // target never transiently drops the old one to zero.
            ObjectType* tmp = m_Pointer;
            m_Pointer = r;
            this->Register();
            if (tmp) tmp->UnRegister();
        }
        return *this;
    }

    ObjectType* Print(std::ostream& os) const {
        if (m_Pointer) (*m_Pointer).Print(os);
        return m_Pointer;
    }

 private:
    ObjectType* m_Pointer;

    void Register()   { if (m_Pointer) m_Pointer->Register(); }
    void UnRegister() { if (m_Pointer) m_Pointer->UnRegister(); }
};

template <typename T>
std::ostream& operator<<(std::ostream& os, SmartPointer<T> p) {
    p.Print(os);
    return os;
}

}  // namespace igtl

#endif  // __igtlSmartPointer_h
