// igtlObject.cxx — thin subclass of LightObject. Upstream adds
// observer callbacks + a modified-time counter here; we stub both
// out since no shim-covered class uses them.

#include "igtl/igtlObject.h"
#include "igtl/igtlObjectFactory.h"

namespace igtl {

Object::Pointer Object::New() {
    Pointer sp = ObjectFactory<Object>::Create();
    if (sp.GetPointer() == nullptr) {
        sp = new Object;
    }
    sp->UnRegister();
    return sp;
}

}  // namespace igtl
