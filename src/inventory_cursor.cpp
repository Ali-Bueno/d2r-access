#include "inventory_cursor.h"
#include <D2RLPlugin/context.h>
#include <D2RLPlugin/inventory.h>
#include <D2RLPlugin/item.h>

namespace D2RAccess::InventoryCursor {
namespace {
const D2RL::InventoryService* Inventory(const D2RL::PluginContext* context) {
	const D2RL::InventoryService* inventory = nullptr;
	if (!context || context->QueryService(&inventory)
		!= D2RL::ServiceQueryResult::Success
		|| !D2RL::HasInventoryServiceField(inventory, D2RL::InventoryServiceRequiredSize)
		|| !inventory->getLocalPlayer) return nullptr;
	return inventory;
}
// inventory.h permits capturing the player and reading its items on the UI
// thread. Reacquire the handle each time so a new session cannot leave stale state.
D2RL::PlayerHandle LocalPlayer(const D2RL::PluginContext* context, const D2RL::InventoryService* inventory) {
	D2RL::PlayerHandle player = D2RL::InvalidPlayerHandle;
	if (inventory->getLocalPlayer(context, &player) != D2RL::Inventory::Result::Success) return D2RL::InvalidPlayerHandle;
	return player;
}
std::optional<D2RL::ItemHandle> CursorItem(const D2RL::PluginContext* context) {
	const auto* inventory = Inventory(context);
	if (!inventory || !inventory->getCursorItem) return std::nullopt;
	const auto player = LocalPlayer(context, inventory);
	if (player == D2RL::InvalidPlayerHandle) return std::nullopt;
	D2RL::ItemHandle item = D2RL::InvalidItemHandle;
	const auto result = inventory->getCursorItem(context, player, &item);
	if (result == D2RL::Inventory::Result::NotFound) return D2RL::InvalidItemHandle;
	if (result != D2RL::Inventory::Result::Success) return std::nullopt;
	return item;
}
ItemState State(const D2RL::Items::ItemInfo& info) {
	return {info.runtimeId, info.classId, static_cast<uint32_t>(info.quality), info.socketCount, info.socketedItemCount, info.quantity};
}
struct Search { uint32_t id = 0; std::optional<ItemState> found; };
auto __cdecl Match(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item, void* userData) noexcept
	-> D2RL::Inventory::IterationAction {
	auto* search = static_cast<Search*>(userData);
	if (!search || !item || item->structSize < D2RL::Items::ItemInfoRequiredSize || item->runtimeId != search->id)
		return D2RL::Inventory::IterationAction::Continue;
	search->found = State(*item);
	return D2RL::Inventory::IterationAction::Stop;
}
struct Presence { uint32_t code = 0; bool found = false; };
auto __cdecl HasCode(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item, void* userData) noexcept
	-> D2RL::Inventory::IterationAction {
	auto* presence = static_cast<Presence*>(userData);
	if (!presence || !item || item->structSize < D2RL::Items::ItemInfoRequiredSize || item->code != presence->code)
		return D2RL::Inventory::IterationAction::Continue;
	presence->found = true;
	return D2RL::Inventory::IterationAction::Stop;
}
}

std::optional<bool> HoldingItem(const D2RL::PluginContext* context) {
	const auto item = CursorItem(context);
	if (!item) return std::nullopt;
	return *item != D2RL::InvalidItemHandle;
}

std::optional<ItemState> Held(const D2RL::PluginContext* context) {
	const auto item = CursorItem(context);
	if (!item) return std::nullopt;
	if (*item == D2RL::InvalidItemHandle) return ItemState{};
	const D2RL::ItemService* items = nullptr;
	if (context->QueryService(&items) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasItemServiceField(items, D2RL::ItemServiceRequiredSize) || !items->getItemInfo) return std::nullopt;
	D2RL::Items::ItemInfo info{};
	info.structSize = D2RL::Items::ItemInfoSize;
	if (items->getItemInfo(context, *item, &info) != D2RL::Items::Result::Success) return std::nullopt;
	return State(info);
}

std::optional<uint32_t> HeldItemId(const D2RL::PluginContext* context) {
	const auto held = Held(context);
	if (!held) return std::nullopt;
	return held->id;
}

std::optional<ItemState> Find(const D2RL::PluginContext* context, uint32_t runtimeId) {
	using D2RL::Items::ContainerBit;
	using Container = D2RL::Items::ItemContainer;
	const auto* inventory = Inventory(context);
	if (!runtimeId || !inventory || !inventory->forEachInventoryItem) return std::nullopt;
	const auto player = LocalPlayer(context, inventory);
	if (player == D2RL::InvalidPlayerHandle) return std::nullopt;
	const D2RL::Inventory::ItemFilter filter{
		.structSize = static_cast<uint32_t>(sizeof(D2RL::Inventory::ItemFilter)),
		.flags = 0,
		.containerMask = ContainerBit(Container::Equipment) | ContainerBit(Container::Belt) | ContainerBit(Container::Inventory)
			| ContainerBit(Container::Cube) | ContainerBit(Container::Trade) | ContainerBit(Container::PersonalStash)
			| ContainerBit(Container::SharedStash) | ContainerBit(Container::CustomPage),
		.reserved = 0};
	Search search{runtimeId};
	if (inventory->forEachInventoryItem(context, player, &filter, Match, &search) != D2RL::Inventory::Result::Success)
		return std::nullopt;
	return search.found;
}

bool Holds(const D2RL::PluginContext* context, D2RL::Items::ItemContainer container, uint32_t code) {
	const auto* inventory = Inventory(context);
	if (!code || !inventory || !inventory->forEachInventoryItem) return false;
	const auto player = LocalPlayer(context, inventory);
	if (player == D2RL::InvalidPlayerHandle) return false;
	const D2RL::Inventory::ItemFilter filter{
		.structSize = static_cast<uint32_t>(sizeof(D2RL::Inventory::ItemFilter)),
		.flags = 0,
		.containerMask = D2RL::Items::ContainerBit(container),
		.reserved = 0};
	Presence presence{code};
	return inventory->forEachInventoryItem(context, player, &filter, HasCode, &presence) == D2RL::Inventory::Result::Success
		&& presence.found;
}
}
