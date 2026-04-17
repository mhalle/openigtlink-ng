// GENERATED — do not edit. Regenerate with: uv run oigtl-corpus codegen cpp
//
// Populates a Registry with every wire type_id this codegen knows
// about. Schemas are listed alphabetically by type_id for stable
// diffs across regenerations.
#ifndef OIGTL_MESSAGES_REGISTER_ALL_HPP
#define OIGTL_MESSAGES_REGISTER_ALL_HPP

#include "oigtl/runtime/dispatch.hpp"

namespace oigtl::messages {

// Register all generated message codecs into *registry*.
void register_all(oigtl::runtime::Registry& registry);

// Convenience: returns a Registry pre-populated with every
// generated message codec.
oigtl::runtime::Registry default_registry();

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_REGISTER_ALL_HPP
