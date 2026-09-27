#include "native_ground_items.h"
#include "layout_check.h"
#include "ground_item_layout.h"
#include "ground_item_text.h"
#include "native_memory.h"
#include "localization.h"
#include "ui_text.h"
#include "async_log.h"
#include <D2RLPlugin/item.h>
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>

namespace D2RAccess::NativeGroundItems {
namespace {
using namespace GroundItemLayout;
using NativeMemory::Peek;
uintptr_t g_base = 0;
struct Tables { uintptr_t items = 0, types = 0; uint64_t itemCount = 0, typeCount = 0; };
}
bool RuntimeRoots(uintptr_t& player, uintptr_t& tables) noexcept {
	__try {
		const auto index = reinterpret_cast<uint32_t(*)()>(g_base + LocalIndex)();
		if (index >= 8) return false; // bound in native 0x9A480
		player = reinterpret_cast<uintptr_t(*)(uint32_t)>(g_base + LocalPlayer)(index);
		uint8_t bank = 0;
		if (!player || !Peek(player + UnitBank, bank) || bank > 3) return false;
		tables = reinterpret_cast<uintptr_t(*)(uint8_t)>(g_base + TableBank)(bank);
		return tables != 0;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
namespace {
int StatAmount(uintptr_t unit, int stat) noexcept {
	if (!g_base) return 0;
	__try { return reinterpret_cast<int(*)(uintptr_t, int, uint32_t)>(g_base + GetStat)(unit, stat, 0); }
	__except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
int GoldAmount(uintptr_t unit) noexcept { return StatAmount(unit, GoldStat); }
bool IsType(const Tables& tables, uint16_t index, uint32_t code) {
	std::vector<uint16_t> pending{index};
	for (size_t i = 0; i < pending.size(); ++i) {
		if (!pending[i] || pending[i] >= tables.typeCount) continue;
		TypeRecord row{};
		if (!Peek(tables.types + pending[i] * TypeStride, row)) continue;
		if (row.code == code) return true;
		for (const auto parent : {row.parent1, row.parent2})
			if (parent && pending.size() < MaxTypeDepth && std::find(pending.begin(), pending.end(), parent) == pending.end()) pending.push_back(parent);
	}
	return false;
}
bool ReadItem(uintptr_t unit, uintptr_t room, const Tables& tables, GroundItems::Point player, GroundItems::Item& item) {
	uint32_t type = 0, mode = 0, classId = 0;
	uintptr_t path = 0, itemRoom = 0, data = 0;
	int x = 0, y = 0;
	if (!Peek(unit + UnitType, type) || type != ItemUnitType || !Peek(unit + UnitMode, mode)
		|| mode != OnGround || !Peek(unit + UnitPath, path) || !path
		|| !Peek(path, itemRoom) || itemRoom != room || !Peek(path + StaticX, x) || !Peek(path + StaticY, y)
		|| x < 0 || y < 0 || x > UINT16_MAX || y > UINT16_MAX) return false;
	item.position = {static_cast<double>(x), static_cast<double>(y)};
	const double distance = std::hypot(item.position.x - player.x, item.position.y - player.y);
	if (distance > GroundItems::QuestAudibleRadius) return false;
	if (!Peek(unit + UnitClass, classId) || classId >= tables.itemCount || !Peek(unit + UnitId, item.id)
		|| !Peek(unit + UnitData, data) || !data || !Peek(data + ItemQuality, item.quality)) return false;
	const uintptr_t row = tables.items + classId * ItemStride;
	uint16_t nameId = 0, typeId = 0;
	uint8_t questFlag = 0;
	if (!Peek(row + ItemCode, item.code) || !Peek(row + ItemName, nameId)
		|| !Peek(row + ItemType, typeId) || !Peek(row + ItemQuest, questFlag)) return false;
	using GroundItems::Base;
	using D2RL::Items::MakeItemCode; // space padded, as the game stores "gem "
	const auto is = [&](uint32_t code) { return IsType(tables, typeId, code); };
	const bool gold = is(MakeItemCode("gold"));
	const Base base = gold ? Base::Gold : is(MakeItemCode("poti")) || is(MakeItemCode("elix")) ? Base::Potion
		: is(MakeItemCode("scro")) ? Base::Scroll : is(MakeItemCode("rune")) ? Base::Rune
		: is(MakeItemCode("gem")) ? Base::Gem : Base::Equipment;
	const bool quest = questFlag != 0 || is(MakeItemCode("ques"));
	item.sound = GroundItems::Classify(item.quality, base, quest);
	if (distance > GroundItems::AudibleRange(item.sound)) return false;
	item.baseName = GroundItemText::BaseName(UiText::Clean(Localization::Text(nameId)));
	item.name = GroundItemText::Describe(item.baseName, item.quality, gold ? GoldAmount(unit) : 0);
	return true;
}
}
std::string ItemBaseName(uintptr_t unit) {
	uintptr_t player = 0, tables = 0, items = 0;
	uint64_t count = 0;
	uint32_t type = 0, classId = 0;
	uint16_t nameId = 0;
	if (!g_base || !unit || !RuntimeRoots(player, tables)
		|| !Peek(unit + UnitType, type) || type != ItemUnitType || !Peek(unit + UnitClass, classId)
		|| !Peek(tables + Items, items) || !items || !Peek(tables + ItemCount, count) || count > MaxTableRows
		|| classId >= count || !Peek(items + classId * ItemStride + ItemName, nameId)) return {};
	return GroundItemText::BaseName(UiText::Clean(Localization::Text(nameId)));
}
UnitLabel ItemUnitLabel(uintptr_t unit) {
	uintptr_t data = 0;
	uint32_t type = 0, classId = 0, quality = 0;
	if (!g_base || !unit || !Peek(unit + UnitType, type) || type != ItemUnitType || !Peek(unit + UnitClass, classId)
		|| !Peek(unit + UnitData, data) || !data || !Peek(data + ItemQuality, quality)) return {};
	return ClassLabel(classId, quality);
}
UnitLabel ClassLabel(uint32_t classId, uint32_t quality) {
	uintptr_t player = 0, tables = 0, items = 0;
	uint64_t count = 0;
	uint16_t nameId = 0;
	if (!g_base || !RuntimeRoots(player, tables)
		|| !Peek(tables + Items, items) || !items || !Peek(tables + ItemCount, count) || count > MaxTableRows
		|| classId >= count || !Peek(items + classId * ItemStride + ItemName, nameId)) return {};
	return {GroundItemText::BaseName(UiText::Clean(Localization::Text(nameId))), quality};
}
uint32_t ItemTableCode(uintptr_t unit) {
	uintptr_t player = 0, tables = 0, items = 0;
	uint64_t count = 0;
	uint32_t type = 0, classId = 0, code = 0;
	if (!g_base || !unit || !RuntimeRoots(player, tables)
		|| !Peek(unit + UnitType, type) || type != ItemUnitType || !Peek(unit + UnitClass, classId)
		|| !Peek(tables + Items, items) || !items || !Peek(tables + ItemCount, count) || count > MaxTableRows
		|| classId >= count || !Peek(items + classId * ItemStride + ItemCode, code)) return 0;
	return code;
}
uint32_t ClassCode(uint32_t classId) {
	uintptr_t player = 0, tables = 0, items = 0;
	uint64_t count = 0;
	uint32_t code = 0;
	if (!g_base || !RuntimeRoots(player, tables)
		|| !Peek(tables + Items, items) || !items || !Peek(tables + ItemCount, count) || count > MaxTableRows
		|| classId >= count || !Peek(items + classId * ItemStride + ItemCode, code)) return 0;
	return code;
}
bool Initialize(const D2RL::PluginContext* context) {
	g_base = 0;
	if (!context) return false;
	if (!LayoutCheck::Verify(context, context->exeBase, Checks, "items", "ground items disabled")) return false;
	g_base = context->exeBase;
	AsyncLog::Info(context, "[items] Verified native ground item reader.");
	return true;
}
bool ReadPlayer(uintptr_t player, uintptr_t tablesAddress, uint32_t playerId, uint64_t session, GroundItems::Snapshot& output) {
	output = {};
	uint32_t type = 0, id = 0, mode = 0;
	uintptr_t path = 0, room = 0;
	std::array<uint32_t, 2> position{};
	Tables tables;
	if (!player || !Peek(player + UnitType, type) || type != PlayerType || !Peek(player + UnitId, id) || id != playerId
		|| !Peek(player + UnitMode, mode) || mode == Death || mode == Dead
		|| !Peek(player + UnitPath, path) || !path || !Peek(path, position) || !Peek(path + PlayerRoom, room) || !room
		|| !tablesAddress || !Peek(tablesAddress + Items, tables.items) || !tables.items
		|| !Peek(tablesAddress + ItemCount, tables.itemCount) || tables.itemCount > MaxTableRows
		|| !Peek(tablesAddress + Types, tables.types) || !tables.types
		|| !Peek(tablesAddress + TypeCount, tables.typeCount) || tables.typeCount > MaxTableRows) return false;
	GroundItems::Snapshot next{session, {position[0] / PositionScale, position[1] / PositionScale}, {}};
	std::vector<uintptr_t> rooms{room};
	uintptr_t neighbors = 0;
	uint32_t count = 0;
	if (!Peek(room + RoomNeighbors, neighbors) || !Peek(room + RoomNeighborCount, count) || count > MaxRooms) return false;
	std::array<uintptr_t, MaxRooms> adjacent{};
	if (count && !NativeMemory::Copy(neighbors, adjacent.data(), count * sizeof(uintptr_t))) return false;
	for (size_t i = 0; i < count; ++i)
		if (adjacent[i] && std::find(rooms.begin(), rooms.end(), adjacent[i]) == rooms.end()) rooms.push_back(adjacent[i]);
	std::unordered_set<uintptr_t> visited;
	for (const auto candidateRoom : rooms) {
		uintptr_t unit = 0;
		if (!Peek(candidateRoom + RoomFirstUnit, unit)) return false;
		while (unit) {
			if (visited.size() >= MaxUnits || !visited.insert(unit).second) return false;
			GroundItems::Item item;
			if (ReadItem(unit, candidateRoom, tables, next.player, item)) next.items.push_back(std::move(item));
			if (!Peek(unit + UnitNext, unit)) return false;
		}
	}
	output = std::move(next);
	return true;
}
bool Read(uint32_t playerId, uint64_t session, GroundItems::Snapshot& output) {
	uintptr_t player = 0, tables = 0;
	if (!g_base || !RuntimeRoots(player, tables)) { output = {}; return false; }
	return ReadPlayer(player, tables, playerId, session, output);
}
int PlayerGold() noexcept {
	uintptr_t player = 0, tables = 0;
	if (!RuntimeRoots(player, tables) || !player) return -1;
	const int gold = GoldAmount(player);
	return gold < 0 ? -1 : gold;
}

int PlayerStashGold() noexcept {
	uintptr_t player = 0, tables = 0;
	if (!RuntimeRoots(player, tables) || !player) return -1;
	const int gold = StatAmount(player, StashGoldStat);
	return gold < 0 ? -1 : gold;
}

}
