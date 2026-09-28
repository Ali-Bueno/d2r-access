#pragma once

namespace D2RL { struct PluginContext; }

// Says where a bought or taken-off item landed while the inventory is open.
// docs/inventory-npc-followup.md#arrivals
namespace D2RAccess::InventoryArrivalService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context);
}
