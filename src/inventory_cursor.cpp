#include "inventory_cursor.h"
#include <D2RLPlugin/context.h>
#include <D2RLPlugin/inventory.h>
#include <D2RLPlugin/item.h>

namespace D2RAccess::InventoryCursor {
namespace {
std::optional<D2RL::ItemHandle> CursorItem(const D2RL::PluginContext* context) {
	const D2RL::InventoryService* inventory = nullptr;
	if (!context || context->QueryService(&inventory)
		!= D2RL::ServiceQueryResult::Success
		|| !D2RL::HasInventoryServiceField(inventory, D2RL::InventoryServiceRequiredSize)
		|| !inventory->getLocalPlayer || !inventory->getCursorItem) return std::nullopt;
	// inventory.h permits capturing the player and reading its cursor on the UI
	// thread. Reacquire both handles so a new session cannot leave stale state.
	D2RL::PlayerHandle player = D2RL::InvalidPlayerHandle;
	if (inventory->getLocalPlayer(context, &player) != D2RL::Inventory::Result::Success
		|| player == D2RL::InvalidPlayerHandle) return std::nullopt;
	D2RL::ItemHandle item = D2RL::InvalidItemHandle;
	const auto result = inventory->getCursorItem(context, player, &item);
	if (result == D2RL::Inventory::Result::NotFound) return D2RL::InvalidItemHandle;
	if (result != D2RL::Inventory::Result::Success) return std::nullopt;
	return item;
}
}

std::optional<bool> HoldingItem(const D2RL::PluginContext* context) {
	const auto item = CursorItem(context);
	if (!item) return std::nullopt;
	return *item != D2RL::InvalidItemHandle;
}

std::optional<uint32_t> HeldItemId(const D2RL::PluginContext* context) {
	const auto item = CursorItem(context);
	if (!item) return std::nullopt;
	if (*item == D2RL::InvalidItemHandle) return 0u;
	const D2RL::ItemService* items = nullptr;
	if (context->QueryService(&items) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasItemServiceField(items, D2RL::ItemServiceRequiredSize) || !items->getItemInfo) return std::nullopt;
	D2RL::Items::ItemInfo info{};
	info.structSize = D2RL::Items::ItemInfoSize;
	if (items->getItemInfo(context, *item, &info) != D2RL::Items::Result::Success) return std::nullopt;
	return info.runtimeId;
}
}
