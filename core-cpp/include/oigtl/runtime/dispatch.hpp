// Public registry of message body classes, keyed by wire type_id.
//
// The registry stores a single function per wire type_id that
// performs an "unpack then repack" round-trip on a content buffer
// and returns the resulting bytes. This deliberately narrow surface
// is enough for the conformance oracle (verify_wire_bytes) and for
// extension registration: third-party code can register additional
// type_ids without touching core.
//
// Built-in types are populated by the codegen-emitted
// `oigtl::messages::register_all(Registry&)`. The public API below
// matches the Python (`register_message_type`) and TypeScript
// (`registerMessageType`) equivalents so the three languages speak
// the same vocabulary even though the function signatures differ
// (C++ is compile-time typed, Python/TS are runtime typed).
//
// String storage: registry keys are `std::string` (not
// `std::string_view`) because type_id strings often come from wire
// bytes whose lifetime ends before the registry's. We pay one
// allocation per registration; lookups still use a temporary copy.
#ifndef OIGTL_RUNTIME_DISPATCH_HPP
#define OIGTL_RUNTIME_DISPATCH_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "oigtl/runtime/error.hpp"

namespace oigtl::runtime {

// Round-trip a body content buffer: unpack via the appropriate
// Message::unpack, immediately re-pack via Message::pack. Returns
// the canonical bytes the registered message produces — which the
// oracle byte-compares to the input.
using RoundTripFn = std::vector<std::uint8_t> (*)(
    const std::uint8_t* data, std::size_t length);

// Raised by register_message_type when an override is attempted
// without the explicit `override=true` flag. Distinct from the
// wire-format ProtocolError hierarchy because it's a caller-side
// logic issue.
class RegistryConflictError : public std::runtime_error {
 public:
    using std::runtime_error::runtime_error;
};

class Registry {
 public:
    // Register `fn` as the round-trip function for `type_id`.
    //
    // Matches the Python `oigtl.register_message_type` /
    // TypeScript `registerMessageType` semantics: by default, throws
    // RegistryConflictError if `type_id` is already bound to a
    // different function. Re-registering the *same* (type_id, fn)
    // pair is always idempotent. Pass `replace=true` to force
    // replacement (intended for tests and deliberate swaps).
    //
    // `replace` spells the Python/TS `override` kwarg; we avoid
    // that name in C++ because `override` is a contextual keyword
    // elsewhere in the language and confuses some tooling.
    void register_message_type(std::string type_id,
                               RoundTripFn fn,
                               bool replace = false);

    // Remove and return the function bound to `type_id`, or nullptr
    // if unbound. Primarily useful in tests that need to undo a
    // registration between cases.
    RoundTripFn unregister_message_type(std::string_view type_id);

    // Return the function registered for `type_id`, or nullptr.
    // Unlike register_message_type, never throws.
    RoundTripFn lookup_message_class(
        std::string_view type_id) const noexcept;

    // Sorted list of every currently-registered type_id. Useful for
    // tooling (CLI info, docs generators, sanity checks).
    std::vector<std::string> registered_types() const;

    bool contains(std::string_view type_id) const noexcept {
        return lookup_message_class(type_id) != nullptr;
    }

    std::size_t size() const noexcept { return table_.size(); }

 private:
    // Default hash/equality. Heterogeneous lookup (string_view →
    // string key) is C++20; we pay one allocating copy on lookup.
    std::unordered_map<std::string, RoundTripFn> table_;
};

}  // namespace oigtl::runtime

#endif  // OIGTL_RUNTIME_DISPATCH_HPP
