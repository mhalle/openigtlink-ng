// Type-id → message dispatch registry.
//
// The registry stores a single function per wire type_id that
// performs an "unpack then repack" round-trip on a content buffer
// and returns the resulting bytes. This deliberately narrow surface
// is enough for the conformance oracle (verify_wire_bytes below)
// and avoids forcing every caller to know each Message type at
// compile time.
//
// The codegen emits a `oigtl::messages::register_all(Registry&)`
// function that populates the registry with all 84 generated
// types. Applications can also register additional type_ids at
// runtime — useful for extensions outside the core spec.
//
// String storage: registry keys are `std::string` (not
// `std::string_view`) because type_id strings often come from
// wire bytes whose lifetime ends before the registry's. We pay
// one allocation per registration; lookups still use
// transparent comparison via the heterogeneous-lookup helpers in
// the .cpp.
#ifndef OIGTL_RUNTIME_DISPATCH_HPP
#define OIGTL_RUNTIME_DISPATCH_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace oigtl::runtime {

// Round-trip a body content buffer: unpack via the appropriate
// Message::unpack, immediately re-pack via Message::pack. Returns
// the canonical bytes the registered message produces — which the
// oracle byte-compares to the input.
using RoundTripFn = std::vector<std::uint8_t> (*)(
    const std::uint8_t* data, std::size_t length);

class Registry {
public:
    // Register a round-trip function for *type_id*. If a function
    // is already registered for that type_id, it is replaced.
    void register_type(std::string type_id, RoundTripFn fn);

    // Returns nullptr if no function is registered for *type_id*.
    RoundTripFn lookup(std::string_view type_id) const noexcept;

    bool contains(std::string_view type_id) const noexcept {
        return lookup(type_id) != nullptr;
    }

    std::size_t size() const noexcept { return table_.size(); }

private:
    // We use the default std::string hash/equality. Heterogeneous
    // lookup (string_view → string key) is a C++20 feature that
    // isn't available on every C++17 stdlib (notably Apple libc++),
    // so lookup() does one allocating copy. Replace with
    // transparent lookup once we bump to C++20.
    std::unordered_map<std::string, RoundTripFn> table_;
};

}  // namespace oigtl::runtime

#endif  // OIGTL_RUNTIME_DISPATCH_HPP
