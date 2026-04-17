#include "oigtl/runtime/dispatch.hpp"

namespace oigtl::runtime {

void Registry::register_type(std::string type_id, RoundTripFn fn) {
    table_.insert_or_assign(std::move(type_id), fn);
}

RoundTripFn Registry::lookup(std::string_view type_id) const noexcept {
    // Heterogeneous unordered_map lookup landed in C++20 and isn't
    // available on every C++17 stdlib. The cost of an allocating
    // copy here is negligible compared to the unpack/repack work
    // the caller is about to do.
    auto it = table_.find(std::string(type_id));
    return (it == table_.end()) ? nullptr : it->second;
}

}  // namespace oigtl::runtime
