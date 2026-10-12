#include "equipment_census.h"
#include "ground_item_text.h"
#include "native_ground_items.h"

#include <D2RLPlugin/context.h>
#include <D2RLPlugin/inventory.h>
#include <D2RLPlugin/item.h>

namespace D2RAccess::EquipmentCensus {
namespace {
auto __cdecl CollectWorn(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item, void* userData) noexcept
	-> D2RL::Inventory::IterationAction {
	auto* worn = static_cast<std::vector<Vitals::Breakage::Item>*>(userData);
	if (worn && item && item->structSize >= D2RL::Items::ItemInfoRequiredSize)
		worn->push_back({item->runtimeId, item->durability, item->maximumDurability, item->bodyLocation,
			item->classId, static_cast<uint32_t>(item->quality), item->quantity});
	return D2RL::Inventory::IterationAction::Continue;
}
}

Result Read(const D2RL::PluginContext* context, std::vector<Vitals::Breakage::Item>& worn) {
	worn.clear();
	const D2RL::InventoryService* inventory = nullptr;
	if (!context || context->QueryService(&inventory) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasInventoryServiceField(inventory, D2RL::InventoryServiceRequiredSize)
		|| !inventory->getLocalPlayer || !inventory->forEachInventoryItem) return Result::Unavailable;
	// Reacquire the handle every time so a new session cannot leave stale state.
	D2RL::PlayerHandle player = D2RL::InvalidPlayerHandle;
	if (inventory->getLocalPlayer(context, &player) != D2RL::Inventory::Result::Success
		|| player == D2RL::InvalidPlayerHandle) return Result::NoPlayer;
	const D2RL::Inventory::ItemFilter filter{
		.structSize = static_cast<uint32_t>(sizeof(D2RL::Inventory::ItemFilter)),
		.flags = 0,
		.containerMask = D2RL::Items::ContainerBit(D2RL::Items::ItemContainer::Equipment),
		.reserved = 0};
	if (inventory->forEachInventoryItem(context, player, &filter, CollectWorn, &worn)
		!= D2RL::Inventory::Result::Success) { worn.clear(); return Result::Unavailable; }
	return Result::Read;
}

std::string Name(const Vitals::Breakage::Item& item) {
	const auto label = NativeGroundItems::ClassLabel(item.classId, item.quality);
	return GroundItemText::Describe(label.baseName, label.quality, 0);
}
}
