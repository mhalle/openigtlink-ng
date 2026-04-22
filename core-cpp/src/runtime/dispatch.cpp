// Type-id → factory registry for typed message dispatch. See
// dispatch.hpp for the public API (register_message_type /
// lookup_message_class / unregister_message_type) and error model.

#include "oigtl/runtime/dispatch.hpp"

#include <algorithm>
#include <string>

namespace oigtl::runtime {

void Registry::register_message_type(std::string type_id,
                                     RoundTripFn fn,
                                     bool replace) {
    const auto it = table_.find(type_id);
    if (it != table_.end()) {
        if (it->second == fn) {
            // Idempotent: registering the same (type_id, fn) pair
            // twice is a no-op, matching Python / TypeScript.
            return;
        }
        if (!replace) {
            throw RegistryConflictError(
                "type_id \"" + type_id +
                "\" is already registered; pass replace=true to "
                "replace it, or pick a different type_id");
        }
    }
    table_.insert_or_assign(std::move(type_id), fn);
}

RoundTripFn Registry::unregister_message_type(std::string_view type_id) {
    // One allocating copy; see the header note about heterogeneous
    // lookup being a C++20 feature.
    const auto it = table_.find(std::string(type_id));
    if (it == table_.end()) {
        return nullptr;
    }
    const RoundTripFn prior = it->second;
    table_.erase(it);
    return prior;
}

RoundTripFn Registry::lookup_message_class(
    std::string_view type_id) const noexcept {
    const auto it = table_.find(std::string(type_id));
    return (it == table_.end()) ? nullptr : it->second;
}

std::vector<std::string> Registry::registered_types() const {
    std::vector<std::string> out;
    out.reserve(table_.size());
    for (const auto& [key, _] : table_) {
        out.push_back(key);
    }
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace oigtl::runtime
