// CRC-64 ECMA-182, matching the OpenIGTLink reference implementation.
//
// Algorithm and lookup table are the same as
// corpus-tools/.../codec/crc64.py and upstream igtl_util.c
// (pinned SHA 94244fe). Verified bit-exact against all 24 upstream
// test fixtures via the Python oracle.
//
// Usage:
//     std::uint64_t crc = oigtl::runtime::crc64(buf, len);
// or, for chained / multi-region CRCs (used by IMAGE, POLYDATA):
//     crc = oigtl::runtime::crc64(region1_data, region1_len);
//     crc = oigtl::runtime::crc64(region2_data, region2_len, crc);
#ifndef OIGTL_RUNTIME_CRC64_HPP
#define OIGTL_RUNTIME_CRC64_HPP

#include <cstddef>
#include <cstdint>

namespace oigtl::runtime {

std::uint64_t crc64(const std::uint8_t* data,
                    std::size_t length,
                    std::uint64_t crc = 0) noexcept;

}  // namespace oigtl::runtime

#endif  // OIGTL_RUNTIME_CRC64_HPP
