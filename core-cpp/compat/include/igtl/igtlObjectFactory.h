// igtlObjectFactory.h — minimal shim. Upstream's factory lets
// plugins register subclasses; we don't support plugin registration,
// so `Create()` always returns null and `igtlNewMacro` falls through
// to `new T`. This is exactly upstream's behaviour with no
// registered factories.
#ifndef __igtlObjectFactory_h
#define __igtlObjectFactory_h

#include "igtlMacro.h"
#include "igtlSmartPointer.h"

namespace igtl {

template <class T>
class ObjectFactory {
 public:
    static SmartPointer<T> Create() { return SmartPointer<T>(); }
};

}  // namespace igtl

#endif  // __igtlObjectFactory_h
