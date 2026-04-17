// TimeStamp shim — wraps the IGTL 32.32 fixed-point uint64.

#include "igtl/igtlTimeStamp.h"

#include <chrono>

namespace igtl {

void TimeStamp::GetTime() {
    const auto now    = std::chrono::system_clock::now();
    const auto since  = now.time_since_epoch();
    const auto secs   =
        std::chrono::duration_cast<std::chrono::seconds>(since);
    const auto nanos  =
        std::chrono::duration_cast<std::chrono::nanoseconds>(since - secs);
    SetTime(static_cast<igtlUint32>(secs.count()),
            static_cast<igtlUint32>(nanos.count()));
}


void TimeStamp::SetTime(igtlUint32 sec, igtlUint32 nanosecond) {
    // Convert nanoseconds to the 32-bit fraction-of-second:
    //   frac = (nanosecond * 2^32) / 1e9
    const igtlUint64 frac =
        (static_cast<igtlUint64>(nanosecond) << 32) /
        1'000'000'000ULL;
    m_TimeStamp = (static_cast<igtlUint64>(sec) << 32) |
                  (frac & 0xFFFFFFFFULL);
}

void TimeStamp::SetTime(double seconds) {
    const igtlUint32 sec = static_cast<igtlUint32>(seconds);
    const igtlUint32 ns  = static_cast<igtlUint32>(
        (seconds - sec) * 1'000'000'000.0);
    SetTime(sec, ns);
}

igtlUint32 TimeStamp::GetSecond() const {
    return static_cast<igtlUint32>(m_TimeStamp >> 32);
}

igtlUint32 TimeStamp::GetNanosecond() const {
    const igtlUint32 frac =
        static_cast<igtlUint32>(m_TimeStamp & 0xFFFFFFFFULL);
    return static_cast<igtlUint32>(
        (static_cast<igtlUint64>(frac) * 1'000'000'000ULL) >> 32);
}

double TimeStamp::GetTimeStamp() {
    return static_cast<double>(GetSecond()) +
           static_cast<double>(GetNanosecond()) / 1e9;
}

void TimeStamp::GetTimeStamp(igtlUint32* sec, igtlUint32* nano) {
    if (sec)  *sec  = GetSecond();
    if (nano) *nano = GetNanosecond();
}

}  // namespace igtl
