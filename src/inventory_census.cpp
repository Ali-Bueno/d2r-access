#include "inventory_census.h"

#include "async_log.h"
#include "native_ground_items.h"
#include "native_widgets.h"

#include <D2RLPlugin/context.h>
#include <D2RLPlugin/inventory.h>
#include <D2RLPlugin/item.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace D2RAccess::InventoryCensus {
namespace {
// A census is 40 item lookups and one loader enumeration; four a second is
// plenty to catch an item being picked up or put down.
constexpr uint64_t IntervalMs = 250;
uint64_t g_next = 0;
// The last map logged per panel, so moving between the player's grid and a
// vendor's does not log each again.
std::unordered_map<std::string, std::string> g_logged;
std::string g_loaderLogged;

// Rows top to bottom, '/' between them: '.' empty, '?' unreadable, and a
// letter per distinct item, named after the map.
std::string GridMap(uintptr_t grid) {
	const auto size = NativeUi::GridSize(grid);
	if (size.x < 1) return {};
	std::vector<uintptr_t> items;
	std::string map = std::to_string(size.x) + "x" + std::to_string(size.y) + " ";
	for (int y = 0; y < size.y; ++y) {
		if (y) map += '/';
		for (int x = 0; x < size.x; ++x) {
			const auto slot = NativeUi::GridItemAt(grid, {x, y});
			if (!slot.known) { map += '?'; continue; }
			if (!slot.item) { map += '.'; continue; }
			auto it = std::find(items.begin(), items.end(), slot.item);
			if (it == items.end()) it = items.insert(items.end(), slot.item);
			const auto index = static_cast<size_t>(it - items.begin());
			map += index < 26 ? static_cast<char>('A' + index) : '#';
		}
	}
	for (size_t index = 0; index < items.size() && index < 26; ++index) {
		const auto name = NativeGroundItems::ItemBaseName(items[index]);
		map += " ";
		map += static_cast<char>('A' + index);
		map += "=" + (name.empty() ? std::string("?") : name);
	}
	return map;
}

struct Entry { std::string code; int x = 0, y = 0; };

auto __cdecl Collect(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item, void* userData) noexcept
	-> D2RL::Inventory::IterationAction {
	auto* entries = static_cast<std::vector<Entry>*>(userData);
	if (entries && item && item->structSize >= D2RL::Items::ItemInfoRequiredSize) {
		Entry entry{{}, item->x, item->y};
		for (int shift = 0; shift < 32; shift += 8) {
			const char c = static_cast<char>(item->code >> shift);
			if (c != ' ' && c != '\0') entry.code += c;
		}
		entries->push_back(std::move(entry));
	}
	return D2RL::Inventory::IterationAction::Continue;
}

// The loader's answer for the player's inventory: each item's table code and
// the cell its top-left corner sits in. Codes give sizes offline (misc.txt,
// weapons.txt, armor.txt invwidth / invheight).
std::string LoaderList(const D2RL::PluginContext* context) {
	const D2RL::InventoryService* inventory = nullptr;
	if (!context || context->QueryService(&inventory) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasInventoryServiceField(inventory, D2RL::InventoryServiceRequiredSize)
		|| !inventory->getLocalPlayer || !inventory->forEachInventoryItem) return {};
	D2RL::PlayerHandle player = D2RL::InvalidPlayerHandle;
	if (inventory->getLocalPlayer(context, &player) != D2RL::Inventory::Result::Success
		|| player == D2RL::InvalidPlayerHandle) return {};
	const D2RL::Inventory::ItemFilter filter{
		.structSize = static_cast<uint32_t>(sizeof(D2RL::Inventory::ItemFilter)),
		.flags = 0,
		.containerMask = D2RL::Items::ContainerBit(D2RL::Items::ItemContainer::Inventory),
		.reserved = 0};
	std::vector<Entry> entries;
	if (inventory->forEachInventoryItem(context, player, &filter, Collect, &entries)
		!= D2RL::Inventory::Result::Success) return {};
	std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
		return a.y != b.y ? a.y < b.y : a.x < b.x;
	});
	std::string list = std::to_string(entries.size()) + " items, top-left x,y:";
	for (const auto& entry : entries)
		list += " " + entry.code + "@" + std::to_string(entry.x) + "," + std::to_string(entry.y);
	return list;
}
}

void Log(const D2RL::PluginContext* context, NativeUi::Focus focus, uint64_t now) {
	if (now < g_next) return;
	g_next = now + IntervalMs;
	const auto map = GridMap(focus.widget);
	if (map.empty()) return;
	const auto panel = NativeWidgets::ReadName(focus.panel);
	if (auto& logged = g_logged[panel]; logged != map) {
		logged = map;
		AsyncLog::InfoF(context, "[grid] %s %s", panel.c_str(), map.c_str());
	}
	if (const auto list = LoaderList(context); !list.empty() && list != g_loaderLogged) {
		g_loaderLogged = list;
		AsyncLog::InfoF(context, "[grid] loader inventory: %s", list.c_str());
	}
}
}
