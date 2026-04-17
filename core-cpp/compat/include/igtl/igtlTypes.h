// igtlTypes.h — fixed-width numeric typedefs + Matrix4x4,
// matching upstream's public surface so message headers and
// consumer code compile unchanged.
#ifndef __igtlTypes_h
#define __igtlTypes_h

#include <cstdint>

// Upstream exposes both CamelCase (`igtlUint8`) and snake_case
// (`igtl_uint8`) spellings. Provide both.
typedef std::int8_t    igtlInt8;     typedef std::int8_t    igtl_int8;
typedef std::int16_t   igtlInt16;    typedef std::int16_t   igtl_int16;
typedef std::int32_t   igtlInt32;    typedef std::int32_t   igtl_int32;
typedef std::int64_t   igtlInt64;    typedef std::int64_t   igtl_int64;
typedef std::uint8_t   igtlUint8;    typedef std::uint8_t   igtl_uint8;
typedef std::uint16_t  igtlUint16;   typedef std::uint16_t  igtl_uint16;
typedef std::uint32_t  igtlUint32;   typedef std::uint32_t  igtl_uint32;
typedef std::uint64_t  igtlUint64;   typedef std::uint64_t  igtl_uint64;
typedef float          igtlFloat32;  typedef float          igtl_float32;
typedef double         igtlFloat64;  typedef double         igtl_float64;

namespace igtl {

// Upstream defines Matrix4x4 as a 4x4 float array. Our underlying
// codec uses 12 floats (top 3 rows of a 4x4 homogeneous transform
// — the bottom row is implicit 0,0,0,1). The shim translates
// between the two shapes at pack/unpack time.
typedef float Matrix4x4[4][4];

}  // namespace igtl

#endif  // __igtlTypes_h
