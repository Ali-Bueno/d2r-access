#pragma once

#include <cstdint>
#include <optional>

namespace D2RL { struct PluginContext; }

namespace D2RAccess::InventoryCursor {
// UI-thread read. Unknown is distinct from an empty cursor; never retain handles.
std::optional<bool> HoldingItem(const D2RL::PluginContext* context);
// The runtime id of the item in hand, 0 when the hand is empty; unknown when unreadable.
std::optional<uint32_t> HeldItemId(const D2RL::PluginContext* context);
}
