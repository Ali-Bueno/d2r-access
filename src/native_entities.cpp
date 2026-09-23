#include "native_entities.h"
#include "generated/lootables.h"
#include "layout_check.h"
#include "entity_layout.h"
#include "ground_item_layout.h"
#include "native_ground_items.h"
#include "level_areas.h"
#include "native_drlg.h"
#include "native_level_areas.h"
#include "native_level_exits.h"
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
// The placeholder label every unnamed MonStats row shares, in the player's
// language. The game's string tables are not loaded when the plugin starts, so
// resolve it on first success and keep it.
const std::string& PlaceholderName() {
	static std::string cached;
	if (cached.empty()) cached = UiText::Clean(Localization::Text(PlaceholderNameKey));
	return cached;
}
// Localized Levels text for a level id: its plain name or its "To The ..." warp caption.
std::string LevelText(const Tables& tables, uint32_t levelId, size_t keyOffset) {
	if (!tables.levels || levelId >= tables.levelCount) return {};
	const auto key = CString(tables.levels + levelId * LevelStride + keyOffset, MaxLevelKeyBytes);
	return key.empty() ? std::string{} : UiText::Clean(Localization::Text(key));
}
// The Level a room belongs to (Room1 -> RoomEx -> Level); null when unreadable.
uintptr_t RoomLevel(uintptr_t room) {
	uintptr_t extended = 0, level = 0;
	if (!Peek(room + RoomExtended, extended) || !extended || !Peek(extended + ExtendedLevel, level)) return 0;
	return level;
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
	entity.category = hostile ? EntityRadar::Category::Enemy : EntityRadar::Category::Npc;
	if (hostile) {
		uint8_t bossFlags = 0, rangedFlags = 0;
		if (Peek(row + MonFlagsBoss, bossFlags)) entity.enemy.important = (bossFlags & (FlagBoss | FlagPrimeEvil)) != 0;
		if (Peek(row + MonFlagsRanged, rangedFlags)) entity.enemy.ranged = (rangedFlags & FlagRanged) != 0;
		// *hcIdx values of the FallenShaman and FetishShaman AI rows in the game's monstats.txt.
		constexpr std::array<uint32_t, 16> ShamanClasses{58, 59, 60, 61, 62, 278, 279, 280, 281, 282,
			645, 646, 647, 662, 663, 664};
		entity.enemy.resurrector = std::find(ShamanClasses.begin(), ShamanClasses.end(), classId) != ShamanClasses.end();
		uint16_t rarity = 0;
		if (Peek(unit + UnitData, data) && data && Peek(data + MonRarityFlags, rarity))
			entity.enemy.important |= (rarity & MonEliteMask) != 0;
	}
	// The game's own composed name (unique/champion titles) when it has been built; base name otherwise.
	uintptr_t cached = 0;
	if (Peek(unit + UnitData, data) && data && Peek(data + MonCachedName, cached)) entity.name = UiText::Clean(CString(cached, MaxNameBytes));
	if (entity.name.empty()) entity.name = UiText::Clean(Localization::Text(nameId));
	if (entity.name.empty()) return false;
	// Rows the game leaves unnamed all resolve to the same placeholder label
	// ("an evil force"): decoration and helper units, never worth announcing.
	const auto& placeholder = PlaceholderName();
	return placeholder.empty() || entity.name != placeholder;
}
// What the object is for, from the table row the caller has already read.
// Order matters: a waypoint and a portal are both exits, but a player asking
// for a waypoint does not want the town portal, so the specific categories
// are tested before the general one.
EntityRadar::Category ObjectCategory(uint32_t classId, uint8_t subClass, bool openWarp, const std::string& key) {
	if (subClass & SubClassWaypoint) return EntityRadar::Category::Waypoint;
	if (subClass & (SubClassPortal | SubClassGate)) return EntityRadar::Category::Portal;
	// Wells and obelisks restore or alter the player the way shrines do, and a
	// player looking for "a shrine" means any of them.
	if (subClass & (SubClassShrine | SubClassWell | SubClassObelisk)) return EntityRadar::Category::Shrine;
	// Everything that gives the player loot, from the game's own objects.txt
	// by way of src/generated/lootables.h. The SubClass container bit is NOT
	// that set and never was: every Act 1 rogue corpse has SubClass 0, and so
	// do Barrel, Urn2, Casket3, Chest4 and Act3DungeonCasket, while Act 2/3
	// corpses carry it -- so the dead rogue a player loots in the Blood Moor
	// was filed beside levers and doors, and some chests were not treasure at
	// all. OperateFn is the real signal; the generated header lists its whole
	// membership. The bit still counts, so a row the generator has not seen
	// classifies exactly as it did before.
	if (Generated::LootableObject(classId) || (subClass & SubClassContainer)) return EntityRadar::Category::Treasure;
	if (openWarp) return EntityRadar::Category::Exit;
	if (key == BankNameKey) return EntityRadar::Category::Stash;
	// Levers, seals, statues and doors carry no SubClass bit of their own.
	return EntityRadar::Category::Interactable;
}

// A world object the player opens for loot. The game's own interaction handler
// says which ones (objects.txt OperateFn), together with the shape every
// container shares: the game lets it be clicked while it is shut and not once
// it is open — which is also why an opened one stops being read at all, two
// lines below. Handler values, and the proof that each offset here equals its
// objects.txt column on all 587 rows: tools/objects_containers.py.
bool IsContainer(uintptr_t row) {
	uint8_t operate = 0, shut = 0, opened = 0;
	if (!Peek(row + ObjOperateFn, operate) || !Peek(row + ObjSelectable + ObjectNeutral, shut)
		|| !Peek(row + ObjSelectable + ObjectOpened, opened) || !shut || opened) return false;
	return std::find(ContainerOperateFns.begin(), ContainerOperateFns.end(), operate) != ContainerOperateFns.end();
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
	entity.container = IsContainer(row);
	const auto key = CString(row + ObjNameKey, MaxKeyBytes);
	// Objects that take the player to another area: portals, gates and waypoints by
	// SubClass, the OpenWarp stairs/trapdoors, and objects named after a level
	// (Duriel's portal, the Ancients' gateway) whose key is a Levels key.
	const bool exit = (subClass & SubClassExit) || openWarp || (tables.levelKeys && tables.levelKeys->contains(key));
	entity.kind = exit ? EntityRadar::Kind::Exit : EntityRadar::Kind::Object;
	entity.category = ObjectCategory(classId, subClass, openWarp != 0, key);
	if (openWarp) entity.levelExitId = NativeLevelExits::Identity(entity.position);
	uintptr_t data = 0;
	if (door) entity.name = UiText::Clean(Localization::Text(mode == ObjectOpened ? DoorOpenText : DoorClosedText));
	if (subClass & SubClassShrine) {
		uintptr_t record = 0;
		uint16_t nameId = 0;
		if (Peek(unit + UnitData, data) && data && Peek(data + ObjLinkRecord, record) && record && Peek(record + ShrineNameId, nameId) && nameId)
			entity.name = UiText::Clean(Localization::Text(nameId));
	}
	if (subClass & SubClassPortal) { // the game labels a portal with its destination level
		// Read as 16 bits: D2RLoader 1.3.0-beta widened this field, and the
		// game's own hover label now consumes it as a word (0xC17E0+167 became
		// `movzx edx, ax`). The offset is unchanged; the field grew into its
		// padding, so a byte-sized read would silently truncate any level above
		// 255 that a future build or a mod introduces.
		uint16_t levelId = 0;
		if (Peek(unit + UnitData, data) && data && Peek(data + ObjPortalLevel, levelId)) {
			entity.name = LevelText(tables, levelId, LevelNameKey);
			entity.destinationLevel = levelId;
		}
	}
	if (entity.name.empty() && !key.empty()) entity.name = UiText::Clean(Localization::Text(key));
	// Spatial cues are derived from the object row, not its translated label.
	// Keep those cues alive while string tables are unavailable or reloading.
	if (entity.name.empty() && entity.kind == EntityRadar::Kind::Exit) entity.name = Strings::Current().exitUnnamed;
	if (entity.name.empty() && entity.container) entity.name = Strings::Current().containerUnnamed;
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
	entity.category = EntityRadar::Category::Exit;
	entity.levelExitId = NativeLevelExits::Identity(entity.position);
	uintptr_t destination = 0, level = 0;
	uint32_t levelId = 0;
	if (Peek(node + WarpNodeDestination, destination) && destination && Peek(destination + ExtendedLevel, level) && level
		&& Peek(level + LevelId, levelId)) {
		entity.name = LevelText(tables, levelId, LevelWarpKey);
		entity.destinationLevel = levelId;
	}
	if (entity.name.empty()) entity.name = Strings::Current().exitUnnamed;
	return true;
}
}

bool Initialize(const D2RL::PluginContext* context) {
	if (!context) { g_base = 0; return false; }
	if (g_base && g_base == context->exeBase) return true; // already verified for this image
	g_base = 0;
	if (!LayoutCheck::Verify(context, context->exeBase, Checks, "scan", "monsters, objects and exits disabled")) return false;
	g_base = context->exeBase;
	AsyncLog::Info(context, "[scan] Verified native monster/object/exit reader.");
	return true;
}
bool ReadPlayer(uintptr_t player, uintptr_t tablesAddress, uint32_t playerId, uint64_t, std::vector<EntityRadar::Entity>& output, bool distantExits, bool levelBearings) {
	const NativeMemory::ReadScope scope;
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
	// The far side of a level passage is a neighbour room too, and it belongs to
	// the other level: its units - that level's own exits above all - would be
	// offered here, thousands of subtiles away behind solid ground. Without a
	// readable level the walk stays as it was.
	const uintptr_t level = RoomLevel(room);
	for (size_t i = 0; i < count; ++i) {
		if (!adjacent[i] || std::find(rooms.begin(), rooms.end(), adjacent[i]) != rooms.end()) continue;
		if (level && RoomLevel(adjacent[i]) != level) continue;
		rooms.push_back(adjacent[i]);
	}
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
	// Permanent level transitions may outlive their loaded warp units. Read
	// the existing presets across this level, then merge by exact position.
	std::vector<NativeLevelExits::Exit> exits;
	if (distantExits && NativeLevelExits::Read(room, exits)) for (const auto& exit : exits) {
		EntityRadar::Entity entity{exit.warp + 1, EntityRadar::Kind::Exit, exit.position,
			LevelText(tables, exit.destination, LevelWarpKey), NativeLevelExits::Identity(exit.position),
			false, EntityRadar::Category::Exit, exit.destination};
		if (entity.name.empty()) entity.name = Strings::Current().exitUnnamed;
		if (std::none_of(entities.begin(), entities.end(), [&](const auto& existing) { return EntityRadar::SameTarget(existing, entity); }))
			entities.push_back(std::move(entity));
	}
	// The other half of "which way out", and the only half a town has: the
	// areas of the act that touch this one. Nothing joins them but the map, so
	// there is no warp for the walk above to find -- see native_level_areas.h.
	// Its own opt-in, not distantExits: a boundary is level-wide, so it is for
	// the catalogue and not for the scan that answers what is nearby.
	// The level id is the identity, so the same boundary is one landmark
	// however the player moves along it.
	std::vector<NativeLevelAreas::Area> areas;
	const uintptr_t levelStruct = RoomLevel(room);
	uint32_t here = 0;
	if (levelBearings && NativeDrlg::Available() && levelStruct && Peek(levelStruct + LevelId, here)
		&& NativeLevelAreas::Read(room, areas)) {
		for (const auto& crossing : LevelAreas::Neighbours(areas, here)) {
			EntityRadar::Entity entity{crossing.level, EntityRadar::Kind::Exit, crossing.position,
				LevelText(tables, crossing.level, LevelNameKey), NativeLevelExits::Identity(crossing.position),
				false, EntityRadar::Category::Exit, crossing.level};
			if (entity.name.empty()) entity.name = Strings::Current().exitUnnamed;
			if (std::none_of(entities.begin(), entities.end(), [&](const auto& existing) { return EntityRadar::SameTarget(existing, entity); }))
				entities.push_back(std::move(entity));
		}
	}
	output = std::move(entities);
	return true;
}
bool Read(uint32_t playerId, uint64_t session, std::vector<EntityRadar::Entity>& output, bool distantExits, bool levelBearings) {
	uintptr_t player = 0, tables = 0;
	if (!g_base || !NativeGroundItems::RuntimeRoots(player, tables)) { output.clear(); return false; }
	return ReadPlayer(player, tables, playerId, session, output, distantExits, levelBearings);
}

bool ReadLayout(uint32_t playerId, bool neighbours, NativeLevelExits::Layout& layout) {
	const NativeMemory::ReadScope scope;
	layout = {};
	uintptr_t player = 0, tables = 0, path = 0, room = 0;
	uint32_t type = 0, id = 0;
	if (!g_base || !NativeGroundItems::RuntimeRoots(player, tables) || !player
		|| !Peek(player + UnitType, type) || type != GroundItemLayout::PlayerType
		|| !Peek(player + UnitId, id) || id != playerId
		|| !Peek(player + UnitPath, path) || !path || !Peek(path + PathRoom, room) || !room) return false;
	return NativeLevelExits::ReadLayout(room, neighbours, layout);
}

std::string LevelName(uint32_t levelId) {
	const NativeMemory::ReadScope scope;
	uintptr_t player = 0, tablesAddress = 0;
	Tables tables;
	if (!g_base || !NativeGroundItems::RuntimeRoots(player, tablesAddress) || !tablesAddress
		|| !Peek(tablesAddress + Levels, tables.levels) || !tables.levels
		|| !Peek(tablesAddress + LevelCount, tables.levelCount) || tables.levelCount > MaxTableRows) return {};
	return LevelText(tables, levelId, LevelNameKey);
}

bool CountContainerRowsAt(uintptr_t tablesAddress, size_t& rows, size_t& total) {
	uintptr_t objects = 0;
	uint64_t count = 0;
	if (!tablesAddress || !Peek(tablesAddress + Objects, objects) || !objects
		|| !Peek(tablesAddress + ObjectCount, count) || count > MaxTableRows) return false;
	rows = 0;
	total = static_cast<size_t>(count);
	for (size_t row = 0; row < total; ++row) rows += IsContainer(objects + row * ObjectStride);
	return true;
}

bool CountContainerRows(size_t& rows, size_t& total) {
	uintptr_t player = 0, tables = 0;
	if (!g_base || !NativeGroundItems::RuntimeRoots(player, tables)) return false;
	return CountContainerRowsAt(tables, rows, total);
}
}
