// libFuzzer entry point for the 58-byte header parse + CRC verify.
//
// Why a dedicated header fuzzer on top of fuzz_oracle.cc:
// the oracle harness gates CRC-invalid inputs out after a few bytes
// of work. This target drives the header path alone, with CRC
// checking on *short* bodies so mutations that craft a valid CRC
// via brute force are still reached efficiently. Any OOB / UAF /
// UBSan hit in unpack_header or verify_crc fails the fuzzer.
//
// Build:
//   cmake -S core-cpp -B core-cpp/build-fuzz \
//         -DBUILD_FUZZERS=ON -DCMAKE_BUILD_TYPE=Debug \
//         -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
//   cmake --build core-cpp/build-fuzz --target fuzz_header
//
// Run (60s smoke on seed corpus):
//   core-cpp/build-fuzz/fuzz_header core-cpp/fuzz/corpus/header \
//       -max_total_time=60 -max_len=4096

#include <cstddef>
#include <cstdint>

#include "oigtl/runtime/error.hpp"
#include "oigtl/runtime/header.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size) {
    // unpack_header itself throws on <58 bytes; skip to avoid
    // burning cycles on the trivial rejection path.
    if (size < oigtl::runtime::kHeaderSize) {
        return 0;
    }

    oigtl::runtime::Header header;
    try {
        header = oigtl::runtime::unpack_header(data, size);
    } catch (const oigtl::error::ProtocolError&) {
        // Expected: malformed / non-ASCII type_id or device_name.
        return 0;
    }

    // CRC verify requires a body. The header declares body_size;
    // if the attacker input also supplies a body, run the CRC
    // over it. A mismatch is expected and non-fatal.
    //
    // Overflow-safe bound: `size - kHeaderSize` is safe (we checked
    // size >= kHeaderSize above), then we compare against body_size
    // without an addition that could wrap for UINT64_MAX inputs.
    // (This is the bug the codec itself had — see
    //  core-cpp/src/runtime/oracle.cpp::parse_wire.)
    const std::size_t body_start = oigtl::runtime::kHeaderSize;
    const std::size_t remaining = size - body_start;
    if (header.body_size > remaining) {
        return 0;
    }
    try {
        oigtl::runtime::verify_crc(
            header, data + body_start, header.body_size);
    } catch (const oigtl::error::ProtocolError&) {
        // Expected when the fuzzer hasn't cracked CRC-64/ECMA-182.
    }
    return 0;
}
