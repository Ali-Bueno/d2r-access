#include "native_entities.h"
#include "entity_layout.h"
#include "ground_item_layout.h"
#include "native_ground_items.h"
#include "native_memory.h"
#include "localization.h"
#include "speech_strings.h"
#include "ui_text.h"
#include "async_log.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_set>

namespace D2RAccess::NativeEntities {
namespace {
using namespace EntityLayout;
using GroundItemLayout::UnitType, GroundItemLayout::UnitClass, GroundItemLayout::UnitId, GroundItemLayout::UnitMode,
	GroundItemLayout::UnitData, GroundItemLayout::UnitPath, GroundItemLayout::UnitNext, GroundItemLayout::StaticX,
	GroundItemLayout::StaticY, GroundItemLayout::RoomNeighbors, GroundItemLayout::RoomNeighborCount,
	GroundItemLayout::RoomFirstUnit, GroundItemLayout::MaxRooms, GroundItemLayout::MaxUnits,
	GroundItemLayout::MaxTableRows, GroundItemLayout::PositionScale;
using NativeMemory::Peek;
uintptr_t g_base = 0;
struct Tables {
	uintptr_t monsters = 0, objects = 0, levels = 0;
	uint64_t monsterCount = 0, objectCount = 0, levelCount = 0;
	const std::unordered_set<std::string>* levelKeys = nullptr; // every LevelName / LevelWarp key
};

// A bounded, printable C string at `address`; empty when unreadable or garbage.
std::string CString(uintptr_t address, size_t maxBytes) {
	std::string text;
	if (!address) return text;
	std::array<char, MaxNameBytes> buffer{};
	maxBytes = std::min(maxBytes, buffer.size());
	// Read byte by byte: the string may end right before an unmapped page.
	for (size_t i = 0; i < maxBytes; ++i) {
		char c = 0;
		if (!Peek(address + i, c)) return {};
		if (c == '\0') return text;
		if (static_cast<unsigned char>(c) < 0x20) return {};
		text.push_back(c);
	}
	return {}; // unterminated within the bound: not a name
}
bool DynamicPosition(uintptr_t unit, uintptr_t room, Radar::Position& position) {
	uintptr_t path = 0, pathRoom = 0;
	std::array<uint32_t, 2> packed{};
	if (!Peek(unit + UnitPath, path) || !path || !Peek(path + PathRoom, pathRoom) || pathRoom != room || !Peek(path, packed)) return false;
	position = {packed[0] / PositionScale, packed[1] / PositionScale};
	return true;
}
bool StaticPosition(uintptr_t unit, uintptr_t room, Radar::Position& position) {
	uintptr_t path = 0, pathRoom = 0;
	int x = 0, y = 0;
	if (!Peek(unit + UnitPath, path) || !path || !Peek(path, pathRoom) || pathRoom != room
		|| !Peek(path + StaticX, x) || !Peek(path + StaticY, y) || x < 0 || y < 0 || x > UINT16_MAX || y > UINT16_MAX) return false;
	position = {static_cast<double>(x), static_cast<double>(y)};
	return true;
}
// The Levels table is static game data: its key set is rebuilt only when the table moves.
const std::unordered_set<std::string>& LevelKeys(uintptr_t levels, uint64_t count) {
	static uintptr_t cachedTable = 0;
	static uint64_t cachedCount = 0;
	static std::unordered_set<std::string> keys;
	if (levels == cachedTable && count == cachedCount) return keys;
	keys.clear();
	for (uint64_t i = 0; i < count; ++i)
		for (const auto offset : {LevelNameKey, LevelWarpKey}) {
			auto key = CString(levels + i * LevelStride + offset, MaxLevelKeyBytes);
			if (!key.empty()) keys.insert(std::move(key));
		}
	cachedTable = levels; cachedCount = count;
	return keys;
}
// Localized Levels text for a level id: its plain name or its "To The ..." warp caption.
std::string LevelText(const Tables& tables, uint32_t levelId, size_t keyOffset) {
	if (!tables.levels || levelId >= tables.levelCount) return {};
	const auto key = CString(tables.levels + levelId * LevelStride + keyOffset, MaxLevelKeyBytes);
	return key.empty() ? std::string{} : UiText::Clean(Localization::Text(key));
}
bool ReadMonster(uintptr_t unit, uintptr_t room, const Tables& tables, EntityRadar::Entity& entity) {
	uint32_t mode = 0, classId = 0, flags = 0;
	uintptr_t data = 0;
	if (!Peek(unit + UnitMode, mode) || mode == MonsterDeath || mode == MonsterDead
		|| !Peek(unit + UnitFlags1, flags) || (flags & (ForcedDead | PlayerMinion))
		|| !Peek(unit + UnitClass, classId) || classId >= tables.monsterCount
		|| !Peek(unit + UnitId, entity.id) || !DynamicPosition(unit, room, entity.position)) return false;
	const uintptr_t row = tables.monsters + classId * MonStatStride;
	uint16_t nameId = 0;
	uint8_t align = 0, flagsA = 0;
	if (!Peek(row + MonNameId, nameId) || !nameId || !Peek(row + MonAlign, align) || !Peek(row + MonFlagsA, flagsA)) return false;
	// The HUD nameplate rule (0xC0420): a hostile plate needs a killable,
	// non-interact monster. Hidden helper units ("an evil force") fail it.
	const bool hostile = align == AlignHostile && !(flagsA & FlagNpc);
	if (hostile && !(flagsA & FlagKillable)) return false;
	entity.kind = hostile ? EntityRadar::Kind::Enemy : EntityRadar::Kind::Npc;
	// The game's own composed name (unique/champion titles) when it has been built; base name otherwise.
	uintptr_t cached = 0;
	if (Peek(unit + UnitData, data) && data && Peek(data + MonCachedName, cached)) entity.name = UiText::Clean(CString(cached, MaxNameBytes));
	if (entity.name.empty()) entity.name = UiText::Clean(Localization::Text(nameId));
	return !entity.name.empty();
}
bool ReadObject(uintptr_t unit, uintptr_t room, const Tables& tables, EntityRadar::Entity& entity) {
	uint32_t mode = 0, classId = 0;
	if (!Peek(unit + UnitMode, mode) || mode >= ObjectModeCount || !Peek(unit + UnitClass, classId) || classId >= tables.objectCount
		|| !Peek(unit + UnitId, entity.id) || !StaticPosition(unit, room, entity.position)) return false;
	const uintptr_t row = tables.objects + classId * ObjectStride;
	uint8_t selectable = 0, door = 0, subClass = 0, openWarp = 0;
	// Only what the game itself lets the player target in the object's current mode.
	if (!Peek(row + ObjSelectable + mode, selectable) || !selectable || !Peek(row + ObjIsDoor, door)
		|| !Peek(row + ObjSubClass, subClass) || !Peek(row + ObjOpenWarp, openWarp)) return false;
	const auto key = CString(row + ObjNameKey, MaxKeyBytes);
	// Objects that take the player to another area: portals, gates and waypoints by
	// SubClass, the OpenWarp stairs/trapdoors, and objects named after a level
	// (Duriel's portal, the Ancients' gateway) whose key is a Levels key.
	const bool exit = (subClass & SubClassExit) || openWarp || (tables.levelKeys && tables.levelKeys->contains(key));
	entity.kind = exit ? EntityRadar::Kind::Exit : EntityRadar::Kind::Object;
	uintptr_t data = 0;
	if (door) entity.name = UiText::Clean(Localization::Text(mode == ObjectOpened ? DoorOpenText : DoorClosedText));
	if (subClass & SubClassShrine) {
		uintptr_t record = 0;
		uint16_t nameId = 0;
		if (Peek(unit + UnitData, data) && data && Peek(data + ObjLinkRecord, record) && record && Peek(record + ShrineNameId, nameId) && nameId)
			entity.name = UiText::Clean(Localization::Text(nameId));
	}
	if (subClass & SubClassPortal) { // the game labels a portal with its destination level
		uint8_t levelId = 0;
		if (Peek(unit + UnitData, data) && data && Peek(data + ObjPortalLevel, levelId)) entity.name = LevelText(tables, levelId, LevelNameKey);
	}
	if (entity.name.empty() && !key.empty()) entity.name = UiText::Clean(Localization::Text(key));
	return !entity.name.empty();
}
// A level exit: the warp unit's node in its room, named after the destination level
// the way the game's hover label does. Walk-through warps the game marks NoInteract
// are skipped: their click proxy is an OpenWarp object read above.
bool ReadWarp(uintptr_t unit, uintptr_t room, const Tables& tables, EntityRadar::Entity& entity) {
	uint32_t classId = 0;
	uintptr_t roomEx = 0, node = 0, row = 0;
	if (!Peek(unit + UnitClass, classId) || !Peek(unit + UnitId, entity.id) || !StaticPosition(unit, room, entity.position)
		|| !Peek(room + RoomExtended, roomEx) || !roomEx || !Peek(roomEx + RoomWarpNodes, node)) return false;
	const uint32_t warpId = classId & WarpIdMask;
	for (size_t i = 0; node && i < MaxWarpNodes; ++i) {
		uint32_t id = 0;
		if (!Peek(node + WarpNodeRow, row) || !row || !Peek(row + LvlWarpId, id)) return false;
		if (id == warpId) break;
		row = 0;
		if (!Peek(node + WarpNodeNext, node)) return false;
	}
	int32_t active = 0;
	uint32_t noInteract = 0;
	if (!row || !Peek(node + WarpNodeActive, active) || !active || !Peek(row + LvlWarpNoInteract, noInteract) || noInteract) return false;
	entity.kind = EntityRadar::Kind::Exit;
	uintptr_t destination = 0, level = 0;
	uint32_t levelId = 0;
	if (Peek(node + WarpNodeDestination, destination) && destination && Peek(destination + ExtendedLevel, level) && level
		&& Peek(level + LevelId, levelId)) entity.name = LevelText(tables, levelId, LevelWarpKey);
	if (entity.name.empty()) entity.name = Strings::Current().exitUnnamed;
	return true;
}
}

bool Initialize(const D2RL::PluginContext* context) {
	if (!context) { g_base = 0; return false; }
	if (g_base && g_base == context->exeBase) return true; // already verified for this image
	g_base = 0;
	for (const auto& check : Checks) {
		std::vector<uint8_t> bytes(check.size);
		if (!NativeMemory::Copy(context->exeBase + check.rva, bytes.data(), bytes.size())) return false;
		uint64_t hash = 14695981039346656037ULL; // FNV-1a of complete native functions
		for (const auto byte : bytes) hash = (hash ^ byte) * 1099511628211ULL;
		if (hash != check.hash) {
			AsyncLog::WarnF(context, "[scan] Unsupported layout at RVA %llX; monsters, objects and exits disabled.", static_cast<unsigned long long>(check.rva));
			return false;
		}
	}
	g_base = context->exeBase;
	AsyncLog::Info(context, "[scan] Verified native monster/object/exit reader.");
	return true;
}
bool ReadPlayer(uintptr_t player, uintptr_t tablesAddress, uint32_t playerId, uint64_t, std::vector<EntityRadar::Entity>& output) {
	output.clear();
	uint32_t type = 0, id = 0;
	uintptr_t path = 0, room = 0;
	Tables tables;
	if (!player || !Peek(player + UnitType, type) || type != GroundItemLayout::PlayerType || !Peek(player + UnitId, id) || id != playerId
		|| !Peek(player + UnitPath, path) || !path || !Peek(path + PathRoom, room) || !room
		|| !tablesAddress || !Peek(tablesAddress + MonStats, tables.monsters) || !tables.monsters
		|| !Peek(tablesAddress + MonStatCount, tables.monsterCount) || tables.monsterCount > MaxTableRows
		|| !Peek(tablesAddress + Objects, tables.objects) || !tables.objects
		|| !Peek(tablesAddress + ObjectCount, tables.objectCount) || tables.objectCount > MaxTableRows) return false;
	// Level names are a refinement: without the table, exits still sound and get the generic name.
	if (!Peek(tablesAddress + Levels, tables.levels) || !Peek(tablesAddress + LevelCount, tables.levelCount) || tables.levelCount > MaxTableRows)
		tables.levels = 0, tables.levelCount = 0;
	if (tables.levels) tables.levelKeys = &LevelKeys(tables.levels, tables.levelCount);
	std::vector<uintptr_t> rooms{room};
	uintptr_t neighbors = 0;
	uint32_t count = 0;
	if (!Peek(room + RoomNeighbors, neighbors) || !Peek(room + RoomNeighborCount, count) || count > MaxRooms) return false;
	std::array<uintptr_t, MaxRooms> adjacent{};
	if (count && !NativeMemory::Copy(neighbors, adjacent.data(), count * sizeof(uintptr_t))) return false;
	for (size_t i = 0; i < count; ++i)
		if (adjacent[i] && std::find(rooms.begin(), rooms.end(), adjacent[i]) == rooms.end()) rooms.push_back(adjacent[i]);
	std::unordered_set<uintptr_t> visited;
	std::vector<EntityRadar::Entity> entities;
	for (const auto candidateRoom : rooms) {
		uintptr_t unit = 0;
		if (!Peek(candidateRoom + RoomFirstUnit, unit)) return false;
		while (unit) {
			if (visited.size() >= MaxUnits || !visited.insert(unit).second) return false;
			uint32_t unitType = 0;
			if (!Peek(unit + UnitType, unitType)) return false;
			EntityRadar::Entity entity;
			if ((unitType == MonsterType && ReadMonster(unit, candidateRoom, tables, entity))
				|| (unitType == ObjectType && ReadObject(unit, candidateRoom, tables, entity))
				|| (unitType == WarpType && ReadWarp(unit, candidateRoom, tables, entity))) entities.push_back(std::move(entity));
			if (!Peek(unit + UnitNext, unit)) return false;
		}
	}
	output = std::move(entities);
	return true;
}
bool Read(uint32_t playerId, uint64_t session, std::vector<EntityRadar::Entity>& output) {
	uintptr_t player = 0, tables = 0;
	if (!g_base || !NativeGroundItems::RuntimeRoots(player, tables)) { output.clear(); return false; }
	return ReadPlayer(player, tables, playerId, session, output);
}
}
