// igtlObject.h — non-lightweight object base. Upstream adds
// callback/observer hooks and a "modified time" counter here;
// neither is used by any message or socket class the shim cares
// about, so we provide the minimal class shape that lets consumers
// subclass it and compile.
#ifndef __igtlObject_h
#define __igtlObject_h

#include "igtlLightObject.h"

namespace igtl {

class IGTLCommon_EXPORT Object : public LightObject {
 public:
    typedef Object                    Self;
    typedef LightObject               Superclass;
    typedef SmartPointer<Self>        Pointer;
    typedef SmartPointer<const Self>  ConstPointer;

    static Pointer New();
    const char* GetNameOfClass() const override { return "Object"; }

    // Upstream exposes a "modified time" counter; we stub it out to
    // zero. Consumers typically call Modified() after mutating state;
    // no-op here — no observers to notify.
    virtual unsigned long GetMTime() const { return 0; }
    virtual void Modified() const { /* no-op */ }

 protected:
    Object() = default;
    ~Object() override = default;
};

}  // namespace igtl

#endif  // __igtlObject_h
