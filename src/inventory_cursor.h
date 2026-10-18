#pragma once

#include <cstdint>
#include <D2RLPlugin/item.h>
#include <optional>

namespace D2RL { struct PluginContext; }

namespace D2RAccess::InventoryCursor {
// UI-thread read. Unknown is distinct from an empty cursor; never retain handles.
std::optional<bool> HoldingItem(const D2RL::PluginContext* context);
// The runtime id of the item in hand, 0 when the hand is empty; unknown when unreadable.
std::optional<uint32_t> HeldItemId(const D2RL::PluginContext* context);
struct ItemState { uint32_t id = 0, classId = 0, quality = 0, sockets = 0, filled = 0; int quantity = 0; };
// The item in hand, id 0 when the hand is empty; unknown when unreadable.
std::optional<ItemState> Held(const D2RL::PluginContext* context);
// One of the local player's own items, worn, carried or stored, by runtime id.
std::optional<ItemState> Find(const D2RL::PluginContext* context, uint32_t runtimeId);
// Whether the local player has an item with this table code in this container.
bool Holds(const D2RL::PluginContext* context, D2RL::Items::ItemContainer container, uint32_t code);
}
