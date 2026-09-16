#include "inventory_cursor.h"
#include <D2RLPlugin/context.h>
#include <D2RLPlugin/inventory.h>

namespace D2RAccess::InventoryCursor {
std::optional<bool> HoldingItem(const D2RL::PluginContext* context) {
	const D2RL::InventoryServiceV1* inventory = nullptr;
	if (!context || context->QueryService(D2RL::ServiceId::Inventory, D2RL::InventoryServiceV1Version, &inventory)
		!= D2RL::ServiceQueryResult::Success
		|| !D2RL::HasInventoryServiceV1Field(inventory, D2RL::InventoryServiceV1RequiredSize)
		|| !inventory->getLocalPlayer || !inventory->getCursorItem) return std::nullopt;
	// inventory.h permits capturing the player and reading its cursor on the UI
	// thread. Reacquire both handles so a new session cannot leave stale state.
	D2RL::PlayerHandle player = D2RL::InvalidPlayerHandle;
	if (inventory->getLocalPlayer(context, &player) != D2RL::Inventory::Result::Success
		|| player == D2RL::InvalidPlayerHandle) return std::nullopt;
	D2RL::ItemHandle item = D2RL::InvalidItemHandle;
	const auto result = inventory->getCursorItem(context, player, &item);
	if (result == D2RL::Inventory::Result::NotFound) return false;
	if (result != D2RL::Inventory::Result::Success) return std::nullopt;
	return item != D2RL::InvalidItemHandle;
}
}
