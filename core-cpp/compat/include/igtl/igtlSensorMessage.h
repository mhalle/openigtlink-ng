// Hand-written facade — supersedes the codegen stub.
//
// SENSOR — 10-byte fixed header + `larray` × 8-byte float64 values.
// Fixed header:
//   1 B  larray   (uint8, array length)
//   1 B  status   (uint8, reserved, ignored by receivers)
//   8 B  unit     (uint64, bit-packed SI-unit descriptor)
// Total body = 10 + larray * 8 bytes.
#ifndef __igtlSensorMessage_h
#define __igtlSensorMessage_h

#include "igtlMacro.h"
#include "igtlMessageBase.h"
#include "igtlTypes.h"

#include <vector>

namespace igtl {

// Upstream's bit-packed SI-unit descriptor — we carry it as an
// opaque uint64 at the wire level. Consumers can decode via
// upstream's igtl::Unit helpers later if needed.
using igtlUnit = igtlUint64;

class IGTLCommon_EXPORT SensorMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::SensorMessage, igtl::MessageBase);
    igtlNewMacro(igtl::SensorMessage);

    int          SetLength(unsigned int n);
    unsigned int GetLength();

    int      SetUnit(igtlUnit u);
    igtlUnit GetUnit() { return m_Unit; }

    int         SetValue(const igtlFloat64* data);
    int         SetValue(unsigned int i, igtlFloat64 v);
    igtlFloat64 GetValue(unsigned int i);

    void SetStatus(igtlUint8 s) { m_Status = s; }
    igtlUint8 GetStatus()        { return m_Status; }

 protected:
    SensorMessage();
    ~SensorMessage() override = default;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    std::vector<igtlFloat64> m_Array;
    igtlUnit                 m_Unit;
    igtlUint8                m_Status;
};

}  // namespace igtl

#endif  // __igtlSensorMessage_h
