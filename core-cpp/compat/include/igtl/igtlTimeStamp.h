// Minimal TimeStamp shim. Upstream's class is more featureful
// (frequency, conversion helpers), but for drop-in compat we only
// need: wrap the IGTL 32.32 fixed-point uint64 and offer the
// set/get overloads that IMGMETA / LBMETA / TRAJ accept.
//
// Ref-counted like all upstream types.
#ifndef __igtlTimeStamp_h
#define __igtlTimeStamp_h

#include "igtlMacro.h"
#include "igtlObject.h"
#include "igtlTypes.h"

namespace igtl {

class IGTLCommon_EXPORT TimeStamp : public Object {
 public:
    igtlTypeMacro(igtl::TimeStamp, igtl::Object);
    igtlNewMacro(igtl::TimeStamp);

    // Store time as IGTL's 32.32 fixed-point uint64 (upper = sec,
    // lower = fraction).
    void SetTime(igtlUint32 sec, igtlUint32 nanosecond);
    void SetTime(igtlUint64 stamp_32_32) { m_TimeStamp = stamp_32_32; }
    void SetTime(double seconds);

    igtlUint32 GetSecond()     const;
    igtlUint32 GetNanosecond() const;
    igtlUint64 GetTimeStampUint64() const { return m_TimeStamp; }

    double GetTimeStamp();
    void   GetTimeStamp(igtlUint32* sec, igtlUint32* nanosecond);

    // Populate from the current system clock — stub: we don't call
    // this during Pack/Unpack, so leave the field unchanged.
    void GetTime() { /* no-op */ }

 protected:
    TimeStamp() : m_TimeStamp(0) {}
    ~TimeStamp() override = default;

    igtlUint64 m_TimeStamp;
};

}  // namespace igtl

#endif  // __igtlTimeStamp_h
