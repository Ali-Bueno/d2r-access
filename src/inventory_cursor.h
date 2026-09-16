#pragma once

#include <optional>

namespace D2RL { struct PluginContext; }

namespace D2RAccess::InventoryCursor {
// UI-thread read. Unknown is distinct from an empty cursor; never retain handles.
std::optional<bool> HoldingItem(const D2RL::PluginContext* context);
}
