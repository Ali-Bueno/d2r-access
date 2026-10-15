#include "inventory_arrival_service.h"
#include "async_log.h"
#include "input_direction.h"
#include "interaction_reader.h"
#include "inventory_arrivals.h"
#include "inventory_cursor.h"
#include "native_ground_items.h"
#include "panel_state.h"
#include "radar_config.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "stash_count.h"
#include <D2RLPlugin/context.h>
#include <D2RLPlugin/inventory.h>
#include <D2RLPlugin/item.h>
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <optional>
#include <utility>
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
std::atomic<bool> g_stashNotice = true;
uint64_t g_nextSample = 0;
std::optional<std::vector<Item>> g_baseline; // what the player held at the last sample, while the inventory is open
bool g_baselineStash = false;                // the baseline holds the stashes too
uint32_t g_heldBefore = 0;

Place PlaceOf(Container container) {
	switch (container) {
	case Container::Inventory: return Place::Backpack;
	case Container::Belt: return Place::Belt;
	case Container::Equipment: return Place::Worn;
	case Container::PersonalStash: return Place::PersonalStash;
	case Container::SharedStash: return Place::SharedStash;
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
// The stashes are read only while the stash is open, the one time their items can move.
std::optional<std::vector<Item>> Read(const D2RL::PluginContext* context, bool stash) {
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
		.containerMask = ContainerBit(Container::Inventory) | ContainerBit(Container::Belt) | ContainerBit(Container::Equipment)
			| (stash ? ContainerBit(Container::PersonalStash) | ContainerBit(Container::SharedStash) : 0U),
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
// The expansion stash has the gem, rune and materials tabs Y sorts into (BankExpansionLayoutHD.json).
constexpr std::string_view ExpansionStash = "BankExpansionLayout";
// A Y press this recent explains an item leaving the backpack: the poll is SampleMs.
constexpr unsigned SortPressMs = 4 * SampleMs;
// How long the gold deposit notice keeps interrupting speech behind it: the length of the sentence.
constexpr uint64_t GoldLeadMs = 2000;
// The same for an item sent into the stash, whose name comes with it.
constexpr uint64_t StashedLeadMs = 2500;
bool StashOpen() {
	const auto& open = PanelState::OpenPanels();
	return std::find(open.begin(), open.end(), ExpansionStash) != open.end();
}
// The tab the game sorts an item into: gems and runes their own, the rest materials.
const char* StashTab(uint32_t classId) {
	using D2RL::Items::MakeItemCode;
	const auto& strings = Strings::Current();
	if (NativeGroundItems::ClassIsType(classId, MakeItemCode("gem"))) return strings.stashGems;
	if (NativeGroundItems::ClassIsType(classId, MakeItemCode("rune"))) return strings.stashRunes;
	return strings.stashMaterials;
}
void AnnounceSorted(const D2RL::PluginContext* context, const std::vector<Item>& before, const std::vector<Item>& now, uint32_t held) {
	if (!StashOpen() || InputDirection::LastFaceButton(SortPressMs) != InputDirection::FaceButton::Y) return;
	for (const auto& item : InventoryArrivals::Departures(before, now, held)) {
		const auto name = InteractionReader::ItemName(item.id, item.classId, item.quality);
		char text[300]{};
		std::snprintf(text, sizeof(text), Strings::Current().itemMovedToStash, name.c_str(), StashTab(item.classId));
		AsyncLog::InfoF(context, "[items] sorted into the stash id=%u class=%u -> \"%s\"", item.id, item.classId, text);
		ScreenReader::Speak(text, false);
	}
}
// An item sent from the backpack straight into a stash, named with the stash as gold is,
// and leading for the same reason: the focus re-reads the cell it left.
void AnnounceStashed(const D2RL::PluginContext* context, const std::vector<Item>& before, const std::vector<Item>& now, uint32_t held) {
	if (!g_stashNotice.load()) return;
	for (const auto& item : InventoryArrivals::Stashed(before, now, held)) {
		const auto name = InteractionReader::ItemName(item.id, item.classId, item.quality);
		const auto& strings = Strings::Current();
		char text[300]{};
		std::snprintf(text, sizeof(text), item.place == Place::SharedStash ? strings.itemMovedToSharedStash
			: strings.itemMovedToPersonalStash, name.c_str());
		AsyncLog::InfoF(context, "[items] stashed id=%u place=%d -> \"%s\"", item.id, static_cast<int>(item.place), text);
		ScreenReader::Lead(text, GetTickCount64() + StashedLeadMs);
	}
}
// Gold that leaves the backpack while the stash is open went into it. The notice leads, so the gold row's
// re-read of what is left (the focus is still on it) waits behind it instead of cutting it off.
int g_gold = -1;
void AnnounceGoldDeposit(const D2RL::PluginContext* context) {
	if (!StashOpen()) { g_gold = -1; return; }
	const int gold = NativeGroundItems::PlayerGold();
	if (gold < 0) return;
	const int before = std::exchange(g_gold, gold);
	if (before < 0 || gold >= before) return;
	const auto& strings = Strings::Current();
	const auto tab = InteractionReader::OpenStashTab(context);
	const char* format = tab == StashCount::GoldTab::Personal ? strings.goldMovedPersonal
		: tab == StashCount::GoldTab::Shared ? strings.goldMovedShared : strings.goldMovedStash;
	char text[160]{};
	std::snprintf(text, sizeof(text), format, std::to_string(before - gold).c_str());
	AsyncLog::InfoF(context, "[items] gold deposited %d -> %d tab=%d -> \"%s\"", before, gold, static_cast<int>(tab), text);
	ScreenReader::Lead(text, GetTickCount64() + GoldLeadMs);
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
	if (!g_enabled || !InventoryOpen()) { g_baseline.reset(); g_gold = -1; return; }
	const uint64_t now = GetTickCount64();
	if (now < g_nextSample) return;
	g_nextSample = now + SampleMs;
	// A baseline read with the stash closed has no stash items to compare, and the other way round.
	const bool stash = StashOpen();
	if (stash != g_baselineStash) g_baseline.reset();
	g_baselineStash = stash;
	auto items = Read(context, stash);
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
	if (g_baseline) AnnounceSorted(context, *g_baseline, *items, *held);
	if (g_baseline) AnnounceStashed(context, *g_baseline, *items, g_heldBefore);
	AnnounceGoldDeposit(context);
	g_baseline = std::move(*items);
	g_heldBefore = *held;
}

bool StashNotice() { return g_stashNotice.load(); }
void SetStashNotice(bool on) { g_stashNotice = on; }
}
