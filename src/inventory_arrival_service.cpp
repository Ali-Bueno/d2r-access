#include "inventory_arrival_service.h"
#include "async_log.h"
#include "interaction_reader.h"
#include "inventory_arrivals.h"
#include "inventory_cursor.h"
#include "panel_state.h"
#include "radar_config.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include <D2RLPlugin/context.h>
#include <D2RLPlugin/inventory.h>
#include <D2RLPlugin/item.h>
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace D2RAccess::InventoryArrivalService {
namespace {
using InventoryArrivals::Item;
using InventoryArrivals::Place;
using Container = D2RL::Items::ItemContainer;
constexpr uint64_t SampleMs = 100; // a purchase answers a button press
constexpr std::string_view InventoryPanel = "PlayerInventoryExpansionLayout";
std::atomic<bool> g_enabled = true;
uint64_t g_nextSample = 0;
std::optional<std::vector<Item>> g_baseline; // what the player held at the last sample, while the inventory is open
uint32_t g_heldBefore = 0;

Place PlaceOf(Container container) {
	switch (container) {
	case Container::Inventory: return Place::Backpack;
	case Container::Belt: return Place::Belt;
	case Container::Equipment: return Place::Worn;
	default: return Place::Other;
	}
}
auto __cdecl Collect(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* info, void* userData) noexcept
	-> D2RL::Inventory::IterationAction {
	auto* items = static_cast<std::vector<Item>*>(userData);
	if (items && info && info->structSize >= D2RL::Items::ItemInfoRequiredSize)
		items->push_back({info->runtimeId, info->classId, static_cast<uint32_t>(info->quality), PlaceOf(info->container), info->x, info->y});
	return D2RL::Inventory::IterationAction::Continue;
}
std::optional<std::vector<Item>> Read(const D2RL::PluginContext* context) {
	using D2RL::Items::ContainerBit;
	const D2RL::InventoryService* inventory = nullptr;
	if (!context || context->QueryService(&inventory) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasInventoryServiceField(inventory, D2RL::InventoryServiceRequiredSize)
		|| !inventory->getLocalPlayer || !inventory->forEachInventoryItem) return std::nullopt;
	D2RL::PlayerHandle player = D2RL::InvalidPlayerHandle;
	if (inventory->getLocalPlayer(context, &player) != D2RL::Inventory::Result::Success
		|| player == D2RL::InvalidPlayerHandle) return std::nullopt;
	const D2RL::Inventory::ItemFilter filter{
		.structSize = static_cast<uint32_t>(sizeof(D2RL::Inventory::ItemFilter)),
		.flags = 0,
		.containerMask = ContainerBit(Container::Inventory) | ContainerBit(Container::Belt) | ContainerBit(Container::Equipment),
		.reserved = 0};
	std::vector<Item> items;
	if (inventory->forEachInventoryItem(context, player, &filter, Collect, &items) != D2RL::Inventory::Result::Success)
		return std::nullopt;
	return items;
}
std::string Location(const Item& item) {
	const auto& strings = Strings::Current();
	if (item.place == Place::Belt) return strings.belt;
	char cell[100]{};
	std::snprintf(cell, sizeof(cell), strings.gridCell, item.y + 1, item.x + 1);
	return cell;
}
bool InventoryOpen() {
	const auto& open = PanelState::OpenPanels();
	return std::find(open.begin(), open.end(), InventoryPanel) != open.end();
}
auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command || !command->plugin) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view args(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (args == "on") g_enabled = true;
	else if (args == "off") g_enabled = false;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_enabled = RadarConfig::ReadEnabled(context, "[inventory_arrivals]").value_or(true);
	(void)context->RegisterConsoleCommand("access-arrivals", Console, "Say where a bought or taken-off item landed: on|off.");
	return true;
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_enabled || !InventoryOpen()) { g_baseline.reset(); return; }
	const uint64_t now = GetTickCount64();
	if (now < g_nextSample) return;
	g_nextSample = now + SampleMs;
	auto items = Read(context);
	const auto held = InventoryCursor::HeldItemId(context);
	if (!items || !held) return;
	if (g_baseline) for (const auto& arrival : InventoryArrivals::Diff(*g_baseline, *items, g_heldBefore)) {
		const auto& item = arrival.item;
		const auto name = InteractionReader::ItemName(item.id, item.classId, item.quality);
		const auto& strings = Strings::Current();
		char text[300]{};
		std::snprintf(text, sizeof(text), arrival.unequipped ? strings.itemUnequippedTo : strings.itemArrived,
			name.c_str(), Location(item).c_str());
		AsyncLog::InfoF(context, "[items] arrived id=%u place=%d at=%d,%d unequipped=%u -> \"%s\"",
			item.id, static_cast<int>(item.place), item.x, item.y, arrival.unequipped ? 1u : 0u, text);
		// News about the backpack, behind whatever the focus is saying.
		ScreenReader::Speak(text, false);
	}
	g_baseline = std::move(*items);
	g_heldBefore = *held;
}
}
