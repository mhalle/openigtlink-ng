// libFuzzer entry point for the full oracle pipeline.
//
// Drives `verify_wire_bytes` — which internally runs:
//   1. unpack_header
//   2. body bounds check
//   3. CRC verify
//   4. extended-header parse (v2+)
//   5. metadata index + body parse
//   6. per-message-type content unpack + round-trip repack
//
// verify_wire_bytes never throws — it catches ProtocolError
// internally and reports failures via VerifyResult::error. Any
// exception that escapes this harness is either (a) an unexpected
// exception class leaking from a codec (a bug: the ProtocolError
// hierarchy must be exhaustive), or (b) an ASan / UBSan trap from
// a memory-safety issue.
//
// Both are fatal — we let them propagate so libFuzzer records the
// crashing input under core-cpp/build-fuzz/crash-*.
//
// Build + run: see fuzz_header.cc for the boilerplate.

#include <cstddef>
#include <cstdint>

#include "oigtl/messages/register_all.hpp"
#include "oigtl/runtime/oracle.hpp"

namespace {

// One-time-initialize the dispatch registry. libFuzzer invokes
// LLVMFuzzerTestOneInput millions of times; re-registering every
// call would dominate cycles.
const oigtl::runtime::Registry& registry() {
    static const oigtl::runtime::Registry r =
        oigtl::messages::default_registry();
    return r;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size) {
    // check_crc=false: we want libFuzzer to reach the per-message
    // content decode path without having to crack CRC-64 by brute
    // force. The CRC read itself is exercised by fuzz_header (which
    // runs verify_crc over arbitrary bodies); the memory-safety
    // payoff for fuzz_oracle lives past the CRC gate — in extended
    // header parse, metadata decode, and the generated unpack code
    // for each message type.
    (void)oigtl::runtime::oracle::verify_wire_bytes(
        data, size, registry(), /*check_crc=*/false);
    return 0;
}
