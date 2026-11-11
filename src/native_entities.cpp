#include "native_entities.h"
#include "generated/level_flags.h"
#include "generated/lootables.h"
#include "native_quest_flags.h"
#include "layout_check.h"
#include "entity_layout.h"
#include "ground_item_layout.h"
#include "native_ground_items.h"
#include "level_areas.h"
#include "native_drlg.h"
#include "native_level_areas.h"
#include "native_level_exits.h"
#include "preset_map_layout.h"
#include "town_npcs.h"
#include "npc_spawn_rules.h"
#include "cairn_stones.h"
#include "tomb_symbols.h"
#include "quest_seals.h"
#include "native_memory.h"
#include "native_pets.h"
#include "localization.h"
#include "monster_plate.h"
#include "speech_strings.h"
#include "ui_text.h"
#include "async_log.h"
#include "core_slots.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <mutex>
#include <optional>
#include <unordered_set>
#include <utility>

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
// The game's enchantment names and champion kinds, once PlateChecks has passed.
static_assert(std::tuple_size_v<decltype(EntityRadar::EnemyTraits::enchantments)> == MonsterPlate::Slots);
std::optional<MonsterPlate::Tables> g_plate;
bool g_immunities = false; // ImmunityChecks passed: the reader may call UNIT_GetStat
bool g_corpsesSupported = false;
bool g_rosterCorpsesSupported = false;
bool g_playersSupported = false;
bool g_rosterSupported = false;
bool g_socketsSupported = false;
bool g_unbuiltSupported = false; // PresetMapLayout checks passed: presets of rooms not built yet are read
bool g_npcAlertsSupported = false; // NpcAlertChecks passed: the "!" over town NPCs is read
bool g_usableCorpsesSupported = false; // UsableCorpseChecks passed: a Necromancer's corpses are counted
bool g_portalOwnersSupported = false; // PortalOwnerChecks passed: a town portal says whose it is
bool g_widePortalLevel = false; // the loader keeps a portal level's high byte at ObjPortalLevelHigh
std::mutex g_usableMutex;
UsableCorpses g_usable;
constexpr uint32_t NoClass = UINT32_MAX;
std::atomic<uint32_t> g_playerClass{NoClass};
// Each stair's LvlWarp row is logged once, so a log tells from where it was taken (a tester, 2026-10-24).
const D2RL::PluginContext* g_logContext = nullptr;
std::mutex g_warpsMutex;
std::unordered_set<uint64_t> g_warpsLogged;
struct Tables {
	uintptr_t monsters = 0, objects = 0, levels = 0;
	uint64_t monsterCount = 0, objectCount = 0, levelCount = 0;
	const std::unordered_set<std::string>* levelKeys = nullptr; // every LevelName / LevelWarp key
};
// A unit's inventory as getter 0x34A360 returns it: +0x90, or +0x98 while the
// +0xA0 owner reference counts at +8. False when a read fails; null is no inventory.
bool InventoryOf(uintptr_t unit, uintptr_t& inventory) {
	uintptr_t ownerRef = 0;
	if (!Peek(unit + UnitInventoryRef, ownerRef) || !Peek(unit + UnitInventory, inventory)) return false;
	if (!ownerRef) return true;
	uint32_t references = 0;
	uintptr_t owned = 0;
	if (!Peek(ownerRef + InventoryRefCount, references)) return false;
	if (references && !Peek(unit + UnitOwnedInventory, owned)) return false;
	if (references && owned) inventory = owned;
	return true;
}

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
// A corpse that has been raised, exploded or otherwise used carries a udead state.
bool UsedCorpse(uintptr_t unit, uintptr_t tablesAddress) {
	uintptr_t stats = 0, mask = 0;
	int32_t extended = 0;
	uint64_t states = 0;
	if (!Peek(unit + UnitStats, stats)) return true;
	if (!stats) return false;
	if (!Peek(stats + StatsExtended, extended)) return true;
	if (extended >= 0) return false; // no active states at all
	if (!Peek(tablesAddress + UdeadStateMask, mask) || !mask || !Peek(tablesAddress + StateCount, states) || states > MaxStates) return true;
	constexpr size_t MaxWords = (MaxStates + 31) >> 5;
	const size_t words = static_cast<size_t>((states + 31) >> 5);
	std::array<uint32_t, MaxWords> udead{}, active{};
	if (!NativeMemory::Copy(mask, udead.data(), words * sizeof(uint32_t))
		|| !NativeMemory::Copy(stats + StatsActiveStates, active.data(), words * sizeof(uint32_t))) return true;
	for (size_t i = 0; i < words; ++i) if (udead[i] & active[i]) return true;
	return false;
}
// The player's table bank, as ReadMonster reads every monster's row: a monster and the
// player it is near share the game's bank (Unit+0x1BD, docs/summons-and-corpses-re.md §8).
bool UsableCorpse(uintptr_t unit, uintptr_t tablesAddress, uint64_t monsterCount, uintptr_t monsters) {
	uint32_t mode = 0, classId = 0;
	int16_t extra = -1;
	uintptr_t rows = 0;
	uint64_t count = 0;
	uint8_t flags = 0;
	return Peek(unit + UnitMode, mode) && mode == MonsterDead && Peek(unit + UnitClass, classId) && classId < monsterCount
		&& Peek(monsters + classId * MonStatStride + MonStatsExtra, extra) && extra >= 0
		&& Peek(tablesAddress + MonStats2, rows) && rows && Peek(tablesAddress + MonStats2Count, count)
		&& static_cast<uint64_t>(extra) < count && Peek(rows + static_cast<uintptr_t>(extra) * MonStats2Stride + MonStats2Flags, flags)
		&& (flags & CorpseSelectable) && !UsedCorpse(unit, tablesAddress);
}
// Unread flags keep the tower's ways quest ways, as before the flags were known.
bool TowerQuestRunning() {
	const auto flags = NativeQuestFlags::Read(EntityRadar::TowerQuestRow);
	return !flags || EntityRadar::QuestRunning(*flags);
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
// The level an object named after one leads to: the row whose LevelName, else LevelWarp, is its key.
uint32_t LevelNamed(const Tables& tables, const std::string& key) {
	for (const auto offset : {LevelNameKey, LevelWarpKey})
		for (uint64_t id = 1; id < tables.levelCount; ++id)
			if (CString(tables.levels + id * LevelStride + offset, MaxLevelKeyBytes) == key) return static_cast<uint32_t>(id);
	return 0;
}
// *hcIdx of the cow and camel rows in the game's monstats.txt (NameStr "dummy"),
// and the string keys that name them: objects.json "cowa", monsters.json "Camel".
// The Act V battlefields' allied fighters (act5barb1/2, NameStr "Act 5 Combatant") are
// barbarians, ui.json "Barbarian" (a tester, 2026-10-27).
const char* AllyNameKey(uint32_t classId) {
	constexpr uint32_t CowClass = 179, CamelClass = 185, CombatantClass1 = 522, CombatantClass2 = 523;
	if (classId == CombatantClass1 || classId == CombatantClass2) return "Barbarian";
	return classId == CowClass ? "cowa" : classId == CamelClass ? "Camel" : nullptr;
}
// The placeholder label every unnamed MonStats row shares, in the player's
// language. The string tables load after the plugin and reload when the text
// language changes in the menus, so it is resolved again after either.
const std::string& PlaceholderName() {
	static std::string cached;
	static Strings::Language language = Strings::Selected();
	if (cached.empty() || language != Strings::Selected()) {
		cached = UiText::Clean(Localization::Text(PlaceholderNameKey));
		language = Strings::Selected();
	}
	return cached;
}
// Localized Levels text for a level id: its plain name or its "To The ..." warp caption.
std::string LevelKey(const Tables& tables, uint32_t levelId, size_t keyOffset) {
	if (!tables.levels || levelId >= tables.levelCount) return {};
	return CString(tables.levels + levelId * LevelStride + keyOffset, MaxLevelKeyBytes);
}
std::string LevelText(const Tables& tables, uint32_t levelId, size_t keyOffset) {
	const auto key = LevelKey(tables, levelId, keyOffset);
	return key.empty() ? std::string{} : UiText::Clean(Localization::Text(key));
}
// The Level a room belongs to (Room1 -> RoomEx -> Level); null when unreadable.
uintptr_t RoomLevel(uintptr_t room) {
	uintptr_t extended = 0, level = 0;
	if (!Peek(room + RoomExtended, extended) || !extended || !Peek(extended + ExtendedLevel, level)) return 0;
	return level;
}
int Stat(uintptr_t unit, int stat) noexcept {
	__try { return reinterpret_cast<int(*)(uintptr_t, int, uint32_t)>(g_base + GetStat)(unit, stat, 0); }
	__except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
// The player's pierce of each immunity, read once for all the monsters a read finds.
using Pierce = std::array<int, MonsterPlate::Immunities.size()>;
std::optional<Pierce> PlayerPierce(uintptr_t player) {
	if (!g_immunities) return std::nullopt;
	Pierce pierce{};
	for (size_t i = 0; i < pierce.size(); ++i) pierce[i] = Stat(player, MonsterPlate::Immunities[i].pierce);
	return pierce;
}
// A champion's, unique's or super unique's name from its string ids, as the game
// composes it before caching it; empty for an ordinary monster.
std::string ComposedName(uintptr_t data) {
	std::array<uint16_t, MonNameParts> ids{};
	if (!NativeMemory::Copy(data + MonNameIds, ids.data(), sizeof(ids)) || ids[0] == NoText) return {};
	const auto format = Localization::Text(ids[0]);
	if (ids[1] == NoText) return MonsterPlate::Form(format);
	std::vector<std::string> parts;
	for (size_t i = 1; i < ids.size() && ids[i] != NoText; ++i) parts.push_back(Localization::Text(ids[i]));
	return MonsterPlate::Compose(format, parts);
}
std::mutex g_deadMutex;
std::vector<uint32_t> g_deadMonsters;
void NoteDead(uintptr_t unit) {
	uint32_t mode = 0, id = 0;
	if (!Peek(unit + UnitMode, mode) || (mode != MonsterDeath && mode != MonsterDead) || !Peek(unit + UnitId, id)) return;
	std::lock_guard lock(g_deadMutex);
	if (g_deadMonsters.size() < MaxUnits && std::find(g_deadMonsters.begin(), g_deadMonsters.end(), id) == g_deadMonsters.end())
		g_deadMonsters.push_back(id);
}
bool HasOverlay(uintptr_t unit, int32_t id) {
	uintptr_t graphics = 0, node = 0;
	if (!Peek(unit + UnitGraphics, graphics) || !graphics || !Peek(graphics + GraphicsOverlays, node)) return false;
	for (size_t i = 0; node && i < MaxOverlays; ++i) {
		int32_t overlay = 0;
		if (!Peek(node + OverlayId, overlay)) return false;
		if (overlay == id) return true;
		if (!Peek(node + OverlayNext, node)) return false;
	}
	return false;
}
// A player's pet -- a summon, the hireling, a bound demon -- is neither an NPC nor an
// enemy: every summon is Align 1, a bound demon keeps its hostile class, and the
// hireling's own flag marks no summon (docs/summons-and-corpses-re.md §3).
bool ReadMonster(uintptr_t unit, uintptr_t room, const Tables& tables, const std::optional<Pierce>& pierce,
	const std::unordered_set<uint32_t>& pets, EntityRadar::Entity& entity) {
	uint32_t mode = 0, classId = 0, flags = 0;
	uintptr_t data = 0;
	if (!Peek(unit + UnitMode, mode) || mode == MonsterDeath || mode == MonsterDead
		|| !Peek(unit + UnitFlags1, flags) || (flags & (ForcedDead | PlayerMinion))
		|| !Peek(unit + UnitClass, classId) || classId >= tables.monsterCount
		|| !Peek(unit + UnitId, entity.id) || pets.contains(entity.id) || NativePets::PlayerOwned(unit)
		|| !DynamicPosition(unit, room, entity.position)) return false;
	// A level's critters are scenery, hostile Align or not: the larvae of Duriel's lair (a tester, 2026-10-24).
	if (Generated::CritterMonster(classId)) return false;
	const uintptr_t row = tables.monsters + classId * MonStatStride;
	uint16_t nameId = 0;
	uint8_t align = 0, flagsA = 0;
	if (!Peek(row + MonNameId, nameId) || !nameId || !Peek(row + MonAlign, align) || !Peek(row + MonFlagsA, flagsA)) return false;
	// The HUD nameplate rule (0xC0420): a hostile plate needs a killable,
	// non-interact monster. Hidden helper units ("an evil force") fail it.
	const bool hostile = align == AlignHostile && !(flagsA & FlagNpc);
	if (hostile && !(flagsA & FlagKillable)) return false;
	MonsterPlate::Plate plate;
	entity.kind = hostile ? EntityRadar::Kind::Enemy : EntityRadar::Kind::Npc;
	entity.category = hostile ? EntityRadar::Category::Enemy : EntityRadar::Category::Npc;
	if (!hostile && (flagsA & FlagNpc) && g_npcAlertsSupported) entity.npcAlert = HasOverlay(unit, NpcAlertOverlay);
	if (hostile && g_plate && Peek(unit + UnitData, data) && data && Peek(data + MonRarityFlags, entity.enemy.flags)
		&& NativeMemory::Copy(data + MonEnchantments, entity.enemy.enchantments.data(), entity.enemy.enchantments.size()))
		plate = MonsterPlate::Read(entity.enemy.flags, entity.enemy.enchantments, flagsA, *g_plate);
	if (hostile && pierce && (classId < MonsterPlate::UberFirst || classId > MonsterPlate::UberLast)) {
		std::array<int, MonsterPlate::Immunities.size()> resist{};
		for (size_t i = 0; i < resist.size(); ++i) resist[i] = Stat(unit, MonsterPlate::Immunities[i].resist);
		plate.immunities = MonsterPlate::Immune(resist, *pierce);
	}
	if (hostile) {
		uint8_t bossFlags = 0, rangedFlags = 0;
		if (Peek(row + MonFlagsBoss, bossFlags)) {
			entity.enemy.important = (bossFlags & (FlagBoss | FlagPrimeEvil)) != 0;
			entity.enemy.boss = (bossFlags & (FlagBoss | FlagPrimeEvil)) != 0;
		}
		if (Peek(row + MonFlagsRanged, rangedFlags)) entity.enemy.ranged = (rangedFlags & FlagRanged) != 0;
		// The catapults (monstats AI Catapult, *hcIdx 497-500) lob their charged balls from afar, but
		// leave rangedtype empty: the archer's cue too (a tester, 2026-10-27).
		constexpr std::array<uint32_t, 4> CatapultClasses{497, 498, 499, 500};
		entity.enemy.ranged |= std::find(CatapultClasses.begin(), CatapultClasses.end(), classId) != CatapultClasses.end();
		// *hcIdx values of the FallenShaman, FetishShaman, GreaterMummy, MummyGenerator (the
		// Sarcophagus, which spawns mummies) and VileMother (the Flesh Spawner's family, which
		// bears its young until killed) AI rows in the game's monstats.txt.
		constexpr std::array<uint32_t, 32> ShamanClasses{58, 59, 60, 61, 62, 278, 279, 280, 281, 282,
			645, 646, 647, 662, 663, 664, 101, 102, 103, 104, 105, 228, 229, 667, 668, 669, 670,
			298, 299, 300, 675, 676};
		entity.enemy.resurrector = std::find(ShamanClasses.begin(), ShamanClasses.end(), classId) != ShamanClasses.end();
		uint16_t rarity = 0;
		if (Peek(unit + UnitData, data) && data && Peek(data + MonRarityFlags, rarity)) {
			entity.enemy.important |= (rarity & MonEliteMask) != 0;
			entity.enemy.boss |= (rarity & MonSuperUnique) != 0;
		}
	}
	// The game's own composed name (unique/champion titles) when it has been built; base name otherwise.
	uintptr_t cached = 0;
	if (Peek(unit + UnitData, data) && data && Peek(data + MonCachedName, cached)) entity.name = UiText::Clean(CString(cached, MaxNameBytes));
	// Built only once the game has shown it (hover, health plate): before that, and
	// always for a super unique, it is composed here from the same string ids.
	if (entity.name.empty() && data) entity.name = UiText::Clean(ComposedName(data));
	const bool named = !entity.name.empty();
	if (entity.name.empty()) entity.name = UiText::Clean(Localization::Text(nameId));
	if (plate.rank != MonsterPlate::Rank::Normal || !plate.immunities.empty()) {
		const auto& strings = Strings::Current();
		entity.enemy.plate = UiText::Clean(MonsterPlate::Spoken(plate, entity.name, named, {
			[](uint32_t id) { return Localization::Text(id); }, [](std::string_view key) { return Localization::Text(key); },
			strings.monsterUnique, strings.listSeparator}));
	}
	if (entity.name.empty()) return false;
	// Town animals share that unnamed row name, but the game names a cow and a
	// camel in other tables (a tester liked the camp's tents and wagons and asked for its cows).
	if (const char* key = AllyNameKey(classId); key && !hostile) {
		if (auto animal = UiText::Clean(Localization::Text(key)); !animal.empty()) {
			entity.name = std::move(animal);
			return true;
		}
	}
	// Rows the game leaves unnamed all resolve to the same placeholder label
	// ("an evil force"): decoration and helper units, never worth announcing.
	const auto& placeholder = PlaceholderName();
	return placeholder.empty() || entity.name != placeholder;
}
// The name of a MonStats row the npc column marks, empty for any other row.
std::string NpcName(const Tables& tables, uint32_t classId) {
	if (classId >= tables.monsterCount) return {};
	const uintptr_t row = tables.monsters + classId * MonStatStride;
	uint16_t nameId = 0;
	uint8_t flagsA = 0;
	if (!Peek(row + MonFlagsA, flagsA) || !(flagsA & FlagNpc) || !Peek(row + MonNameId, nameId) || !nameId) return {};
	auto name = UiText::Clean(Localization::Text(nameId));
	return name == PlaceholderName() ? std::string{} : name;
}
std::mutex g_townPresetsMutex;
std::string g_townPresets;
std::vector<uint64_t> g_goneTownNpcs;
std::mutex g_gameLevelsMutex;
NpcSpawnRules::GameLevels g_gameLevels; // every level the player's reads have been in, this game
// In a town, every NPC its presets place once is listed from anywhere in it,
// rooms not built yet included. `around` are the player's room and its
// neighbours: other loaded rooms may lack units still there (docs/entity-scan.md#town-npcs).
void AddTownNpcs(uintptr_t room, uintptr_t level, const std::vector<uintptr_t>& around, const Tables& tables,
	const std::vector<TownNpcs::Live>& liveNpcs, uint64_t session, bool shared, std::vector<EntityRadar::Entity>& entities) {
	uint32_t levelId = 0;
	std::vector<NativeLevelExits::MonsterPreset> presets;
	std::vector<NativeLevelExits::ObjectPreset> objects;
	if (!Peek(level + LevelId, levelId) || !Generated::FindLevelFlags(static_cast<int32_t>(levelId)).town
		|| !NativeLevelExits::ReadMonsters(room, presets, g_unbuiltSupported, &objects)) return;
	std::unordered_set<uintptr_t> aroundRooms;
	for (const auto aroundRoom : around)
		if (uintptr_t extended = 0; Peek(aroundRoom + RoomExtended, extended) && extended) aroundRooms.insert(extended);
	std::vector<TownNpcs::Spawn> spawns;
	spawns.reserve(presets.size());
	for (const auto& preset : presets) spawns.push_back({preset.monster, preset.position, aroundRooms.contains(preset.room)});
	// NPCs a marker object places, where the game's own test for it would spawn them (Jerhyn, Natalya).
	std::vector<TownNpcs::Marker> markers;
	for (const auto& object : objects) markers.push_back({object.object, object.position, aroundRooms.contains(object.room)});
	const NpcSpawnRules::ThisGame game{[session](int entered) {
		std::lock_guard lock(g_gameLevelsMutex);
		return g_gameLevels.Entered(session, entered);
	}, shared};
	const TownNpcs::SpawnTest test = [&game](int initFn) { return NpcSpawnRules::Spawns(initFn, NativeQuestFlags::Read, game); };
	const auto placed = TownNpcs::MarkerSpawns(markers, test);
	spawns.insert(spawns.end(), placed.begin(), placed.end());
	auto gone = TownNpcs::MarkersGone(markers, test);
	// A row placed twice is not listed: the log tells which NPC that was (a tester, 2026-10-20: Natalya).
	std::map<uint32_t, int> counts;
	for (const auto& spawn : spawns) ++counts[spawn.monster];
	std::string summary;
	for (const auto& [monster, count] : counts)
		if (!NpcName(tables, monster).empty()) summary += " " + std::to_string(monster) + "x" + std::to_string(count);
	{ std::lock_guard lock(g_townPresetsMutex); g_townPresets = std::move(summary); g_goneTownNpcs = std::move(gone); }
	TownNpcs::Merge(spawns, liveNpcs, [&](uint32_t monster) { return NpcName(tables, monster); }, entities);
}
// A player's corpse, own or another's: its inventory's link ID names the owner,
// the same test the game's corpse pickup makes (docs/player-corpse-re.md). The
// dead owner's own unit lies on the same spot until respawn and is not one.
bool ReadPlayerCorpse(uintptr_t unit, uintptr_t room, uint32_t playerId, bool names, EntityRadar::Entity& entity) {
	uint32_t mode = 0, owner = 0, magic = 0;
	uintptr_t inventory = 0, data = 0;
	if (!Peek(unit + UnitMode, mode) || mode != PlayerCorpseMode
		|| !Peek(unit + UnitId, entity.id) || !entity.id || entity.id == playerId
		|| !InventoryOf(unit, inventory)) return false;
	if (!inventory || !Peek(inventory, magic) || magic != InventoryMagic
		|| !Peek(inventory + InventoryLinkId, owner) || !owner || owner == entity.id
		|| !DynamicPosition(unit, room, entity.position)) return false;
	entity.kind = EntityRadar::Kind::Corpse;
	entity.category = EntityRadar::Category::Corpse;
	const auto& strings = Strings::Current();
	if (owner == playerId) entity.name = strings.corpseOwn;
	// The game labels a corpse with the name in its own PlayerData (0xCE8F0 via 0x134FC0).
	else if (names && Peek(unit + UnitData, data) && data) {
		const auto name = UiText::Clean(CString(data + PlayerName, PlayerNameBytes));
		char text[128]{};
		if (!name.empty()) std::snprintf(text, sizeof text, strings.corpseOf, name.c_str());
		entity.name = text;
	}
	if (entity.name.empty()) entity.name = UiText::Clean(Localization::Text(PlayerCorpseNameKey));
	return !entity.name.empty();
}
// Another living player in the game, by the character name the game stores.
bool ReadOtherPlayer(uintptr_t unit, uintptr_t room, uint32_t playerId, EntityRadar::Entity& entity) {
	uint32_t mode = 0, flags = 0;
	uintptr_t data = 0;
	if (!Peek(unit + UnitMode, mode) || mode == PlayerDeathMode || mode == PlayerCorpseMode
		|| !Peek(unit + UnitFlags1, flags) || (flags & ForcedDead)
		|| !Peek(unit + UnitId, entity.id) || !entity.id || entity.id == playerId
		|| !Peek(unit + UnitData, data) || !data || !DynamicPosition(unit, room, entity.position)) return false;
	entity.name = UiText::Clean(CString(data + PlayerName, PlayerNameBytes));
	entity.kind = EntityRadar::Kind::Player;
	entity.category = EntityRadar::Category::Player;
	return !entity.name.empty();
}
// Party members outside the loaded rooms, where the automap marks them: same
// party, same level, unit not loaded here. The server sends the position only
// beyond 50 subtiles; zero is treated as not received yet.
// The local player's party id, NoParty when alone or unreadable.
uint16_t LocalParty(uintptr_t head, uint32_t playerId) {
	uintptr_t entry = head;
	for (size_t i = 0; entry && i < MaxRosterEntries; ++i) {
		uint32_t id = 0;
		uint16_t party = NoParty;
		if (!Peek(entry + RosterUnitId, id)) return NoParty;
		if (id == playerId) return Peek(entry + RosterParty, party) ? party : NoParty;
		if (!Peek(entry + RosterNext, entry)) return NoParty;
	}
	return NoParty;
}
void ReadRoster(uintptr_t head, uint32_t playerId, uint32_t level, const std::vector<uint32_t>& loaded,
	std::vector<EntityRadar::Entity>& entities) {
	const uint16_t party = LocalParty(head, playerId);
	if (party == NoParty || !level) return;
	uintptr_t entry = head;
	for (size_t i = 0; entry && i < MaxRosterEntries; ++i) {
		EntityRadar::Entity entity;
		uint16_t memberParty = NoParty;
		uint32_t memberLevel = 0;
		int32_t x = 0, y = 0;
		if (Peek(entry + RosterUnitId, entity.id) && entity.id && entity.id != playerId
			&& std::find(loaded.begin(), loaded.end(), entity.id) == loaded.end()
			&& Peek(entry + RosterParty, memberParty) && memberParty == party
			&& Peek(entry + RosterLevel, memberLevel) && memberLevel == level
			&& Peek(entry + RosterX, x) && Peek(entry + RosterY, y)
			&& x > 0 && y > 0 && x <= UINT16_MAX && y <= UINT16_MAX) {
			entity.name = UiText::Clean(CString(entry + RosterName, PlayerNameBytes));
			entity.kind = EntityRadar::Kind::Player;
			entity.category = EntityRadar::Category::Player;
			entity.position = {static_cast<double>(x), static_cast<double>(y)};
			if (!entity.name.empty()) entities.push_back(std::move(entity));
		}
		if (!Peek(entry + RosterNext, entry)) return;
	}
}
// Alone in the game the wider player walk below is skipped.
bool OthersInGame(uintptr_t head, uint32_t playerId) {
	uintptr_t entry = head;
	for (size_t i = 0; entry && i < MaxRosterEntries; ++i) {
		uint32_t id = 0;
		if (!Peek(entry + RosterUnitId, id)) return false;
		if (id && id != playerId) return true;
		if (!Peek(entry + RosterNext, entry)) return false;
	}
	return false;
}
// Work bound for the walk below, the same as the radar's collision walk (native_world.cpp).
constexpr size_t MaxLoadedRooms = 256;
// Other players anywhere in this level's loaded rooms, beyond the scan ring. The game
// places a party member from the roster only when its unit is not loaded, and the
// roster position is stale within 50 subtiles (docs/entity-units-re.md).
void ReadLoadedPlayers(std::vector<uintptr_t> rooms, uintptr_t level, uint32_t playerId,
	std::vector<uint32_t>& loaded, std::vector<EntityRadar::Entity>& entities) {
	const size_t ring = rooms.size();
	std::unordered_set<uintptr_t> units;
	for (size_t i = 0; i < rooms.size(); ++i) {
		uintptr_t neighbors = 0, unit = 0;
		uint32_t count = 0;
		std::array<uintptr_t, MaxRooms> adjacent{};
		if (Peek(rooms[i] + RoomNeighbors, neighbors) && Peek(rooms[i] + RoomNeighborCount, count) && count <= MaxRooms
			&& (!count || NativeMemory::Copy(neighbors, adjacent.data(), count * sizeof(uintptr_t))))
			for (size_t n = 0; n < count && rooms.size() < MaxLoadedRooms; ++n)
				if (adjacent[n] && (!level || RoomLevel(adjacent[n]) == level)
					&& std::find(rooms.begin(), rooms.end(), adjacent[n]) == rooms.end()) rooms.push_back(adjacent[n]);
		if (i < ring || !Peek(rooms[i] + RoomFirstUnit, unit)) continue; // the ring's units are already read
		while (unit && units.size() < MaxUnits && units.insert(unit).second) {
			uint32_t type = 0, id = 0;
			if (!Peek(unit + UnitType, type)) break;
			if (type == GroundItemLayout::PlayerType && Peek(unit + UnitId, id) && id != playerId
				&& std::find(loaded.begin(), loaded.end(), id) == loaded.end()) {
				loaded.push_back(id);
				EntityRadar::Entity entity;
				if (ReadOtherPlayer(unit, rooms[i], playerId, entity)) entities.push_back(std::move(entity));
			}
			if (!Peek(unit + UnitNext, unit)) break;
		}
	}
}
// What the object is for, from the table row the caller has already read.
// Order matters: a waypoint and a portal are both exits, but a player asking
// for a waypoint does not want the town portal, so the specific categories
// are tested before the general one.
EntityRadar::Category ObjectCategory(uint32_t classId, uint8_t subClass, bool openWarp, const std::string& key) {
	if (subClass & SubClassWaypoint) return EntityRadar::Category::Waypoint;
	if ((subClass & (SubClassPortal | SubClassGate)) || Generated::PortalArtObject(classId)) return EntityRadar::Category::Portal;
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
	// Levers, statues and doors carry no SubClass bit of their own.
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
// Whose town portal it is, as the game's label says it; empty when nobody's or unread.
std::string PortalOwner(uintptr_t data, uint32_t portalId) {
	uintptr_t entry = 0;
	if (!g_portalOwnersSupported) return {};
	if (g_rosterSupported && Peek(g_base + RosterHead, entry))
		for (size_t i = 0; entry && i < MaxRosterEntries; ++i) {
			uint32_t local = 0, remote = 0;
			if (!Peek(entry + RosterPortal, local) || !Peek(entry + RosterRemotePortal, remote)) break;
			if (local == portalId || remote == portalId) return UiText::Clean(CString(entry + RosterName, PlayerNameBytes));
			if (!Peek(entry + RosterNext, entry)) break;
		}
	return UiText::Clean(CString(data + ObjPortalOwner, PlayerNameBytes));
}
// The other level a room's walk-through (NoInteract) warp leads to, once the game has linked it;
// zero otherwise. The OpenWarp stairs and trapdoors take the player through that warp.
uint32_t WalkThroughDestination(uintptr_t room) {
	uintptr_t roomEx = 0, node = 0, here = 0;
	uint32_t from = 0;
	if (!Peek(room + RoomExtended, roomEx) || !roomEx || !Peek(roomEx + RoomWarpNodes, node)) return 0;
	if (Peek(roomEx + ExtendedLevel, here) && here) (void)Peek(here + LevelId, from);
	for (size_t i = 0; node && i < MaxWarpNodes; ++i) {
		uintptr_t row = 0, destination = 0, level = 0;
		int32_t active = 0;
		uint32_t noInteract = 0, levelId = 0;
		if (!Peek(node + WarpNodeRow, row) || !row) return 0;
		if (Peek(row + LvlWarpNoInteract, noInteract) && noInteract && Peek(node + WarpNodeActive, active) && active
			&& Peek(node + WarpNodeDestination, destination) && destination && Peek(destination + ExtendedLevel, level) && level
			&& Peek(level + LevelId, levelId) && levelId && levelId != from) return levelId;
		if (!Peek(node + WarpNodeNext, node)) return 0;
	}
	return 0;
}
// The walk-through warp an opened stair or door leads on through: in its own room, else in a
// room beside it, as Travincal's door to the Durance of Hate may be (a tester, 2026-10-27).
uint32_t OpenedWarpDestination(uintptr_t room) {
	if (const uint32_t levelId = WalkThroughDestination(room)) return levelId;
	uintptr_t neighbors = 0;
	uint32_t count = 0;
	std::array<uintptr_t, MaxRooms> adjacent{};
	if (!Peek(room + RoomNeighbors, neighbors) || !Peek(room + RoomNeighborCount, count) || count > MaxRooms
		|| (count && !NativeMemory::Copy(neighbors, adjacent.data(), count * sizeof(uintptr_t)))) return 0;
	for (size_t n = 0; n < count; ++n)
		if (adjacent[n] && adjacent[n] != room)
			if (const uint32_t levelId = WalkThroughDestination(adjacent[n])) return levelId;
	return 0;
}
// Once per opened stair or trapdoor; to=0 when neither its room nor one beside it holds a linked walk-through warp.
void LogUnfolded(const EntityRadar::Entity& entity, uint32_t classId) {
	{
		std::lock_guard lock(g_warpsMutex);
		if (!g_logContext || !g_warpsLogged.insert(entity.placeId ^ (static_cast<uint64_t>(entity.id) << 40)).second) return;
	}
	AsyncLog::InfoF(g_logContext, "[exit] unit=%u object=%u opened: to=%u \"%s\" at=%.0f,%.0f",
		entity.id, classId, entity.destinationLevel, entity.name.c_str(), entity.position.x, entity.position.y);
}
bool ReadObject(uintptr_t unit, uintptr_t room, const Tables& tables, uint64_t session, EntityRadar::Entity& entity) {
	uint32_t mode = 0, classId = 0;
	if (!Peek(unit + UnitMode, mode) || mode >= ObjectModeCount || !Peek(unit + UnitClass, classId) || classId >= tables.objectCount
		|| !Peek(unit + UnitId, entity.id) || !StaticPosition(unit, room, entity.position)) return false;
	const uintptr_t row = tables.objects + classId * ObjectStride;
	// A seal is counted in every mode: the open ones are what the count is made of.
	const uint8_t seal = Generated::SealOrdinal(classId);
	if (seal) QuestSeals::Observe(session, seal, mode != ObjectNeutral);
	if (classId == RogueFountainClass) {
		entity.kind = EntityRadar::Kind::Object;
		entity.category = EntityRadar::Category::Interactable;
		entity.placeId = NativeLevelExits::Identity(entity.position);
		entity.name = Strings::Current().rogueFountain;
		entity.mapLandmark = true;
		return true;
	}
	uint8_t selectable = 0, door = 0, subClass = 0, openWarp = 0, operate = 0;
	// Only what the game itself lets the player target in the object's current mode.
	if (!Peek(row + ObjSelectable + mode, selectable) || !selectable || !Peek(row + ObjIsDoor, door)
		|| !Peek(row + ObjSubClass, subClass) || !Peek(row + ObjOpenWarp, openWarp)
		|| !Peek(row + ObjOperateFn, operate)) return false;
	if (operate == WellOperateFn) subClass |= SubClassWell;
	uint8_t collision = 0;
	uint32_t width = 0, height = 0;
	// Doors use their separate DOOR collision bit; retain their existing sight policy.
	if (!door && Peek(row + ObjHasCollision + mode, collision) && collision
		&& Peek(row + ObjSizeX, width) && Peek(row + ObjSizeY, height)
		&& width > 0 && height > 0 && width <= UINT16_MAX && height <= UINT16_MAX)
		entity.footprint = {width, height};
	entity.container = IsContainer(row);
	entity.corpse = entity.container && Generated::CorpseObject(classId);
	entity.weaponRack = entity.container && Generated::WeaponRackObject(classId);
	entity.well = (subClass & SubClassWell) != 0;
	entity.unreadTome = mode == ObjectNeutral && Generated::TomeObject(classId);
	if (door && mode == ObjectNeutral) entity.shutDoor = Generated::DoorOpenSound(classId);
	else if (door) entity.openedDoor = Generated::DoorOpenSound(classId);
	if (door && mode == ObjectNeutral && Peek(row + ObjSizeX, width) && Peek(row + ObjSizeY, height) && width <= UINT16_MAX && height <= UINT16_MAX)
		entity.doorSpan = std::max(width, height);
	entity.questObject = Generated::QuestItemObject(classId) || Generated::QuestMechanismStep(classId, mode);
	if (operate == TeleportPadOperateFn) entity.padClass = static_cast<uint16_t>(classId);
	entity.secretDoor = operate == SecretDoorOperateFn && mode == ObjectNeutral;
	if (CairnStones::Index(classId) && operate == CairnStoneOperateFn) {
		entity.cairnStone = static_cast<uint8_t>(*CairnStones::Index(classId) + 1);
		entity.questObject = mode == ObjectNeutral;
	}
	const auto key = CString(row + ObjNameKey, MaxKeyBytes);
	// Objects that take the player to another area: portals, gates and waypoints by
	// SubClass, the OpenWarp stairs/trapdoors and the sewer stairs, and objects named after a level
	// (Duriel's portal, the Ancients' gateway) whose key is a Levels key.
	// The sewer stairs are the way through themselves, with no OpenWarp flag (RoomWarpOperateFn).
	const bool warp = openWarp || operate == RoomWarpOperateFn;
	// Listed with the exits too: Tal Rasha's Chamber behind the tomb's broken wall (a tester, 2026-10-24).
	const bool namedExit = tables.levelKeys && tables.levelKeys->contains(key);
	const bool exit = (subClass & SubClassExit) || warp || namedExit;
	entity.kind = exit ? EntityRadar::Kind::Exit : EntityRadar::Kind::Object;
	entity.category = ObjectCategory(classId, subClass, warp || namedExit, key);
	if (namedExit) entity.destinationLevel = LevelNamed(tables, key);
	if (EntityRadar::QuestSource(entity)) entity.category = EntityRadar::Category::Quest;
	// Stairs, waypoints and the stash stand where the level's layout put them.
	if (openWarp || namedExit || entity.category == EntityRadar::Category::Waypoint || entity.category == EntityRadar::Category::Stash)
		entity.placeId = NativeLevelExits::Identity(entity.position);
	uintptr_t data = 0;
	if (door) entity.name = UiText::Clean(Localization::Text(mode == ObjectOpened ? DoorOpenText : DoorClosedText));
	if (subClass & SubClassShrine) {
		uintptr_t record = 0;
		uint16_t nameId = 0;
		if (Peek(unit + UnitData, data) && data && Peek(data + ObjLinkRecord, record) && record && Peek(record + ShrineNameId, nameId) && nameId)
			entity.name = UiText::Clean(Localization::Text(nameId));
	}
	if (subClass & SubClassPortal) { // the game labels a portal with its destination level
		uint8_t low = 0, high = 0;
		if (Peek(unit + UnitData, data) && data && Peek(data + ObjPortalLevel, low)
			&& (!g_widePortalLevel || Peek(data + ObjPortalLevelHigh, high))) {
			const uint32_t levelId = low | static_cast<uint32_t>(high) << 8;
			entity.name = LevelText(tables, levelId, LevelNameKey);
			entity.nameKey = LevelKey(tables, levelId, LevelNameKey);
			entity.destinationLevel = levelId;
			entity.owner = PortalOwner(data, entity.id);
			// A player's portal is listed by its owner alone; where it leads is read on reaching it,
			// so one opened anew is never listed twice under two names (a tester, 2026-10-22).
			if (const auto& owner = entity.owner; !owner.empty()) {
				char text[512]{};
				std::snprintf(text, sizeof text, Strings::Current().portalOwned, owner.c_str());
				entity.name = text;
				entity.nameKey.clear(); // composed
			}
		}
		// Its objects.txt key is "Portal to ", a prefix: no name without the level.
		if (entity.name.empty()) entity.name = Strings::Current().exitUnnamed;
	}
	// Unfolded or opened, stairs and trapdoors are named after where they lead, as a warp is: each
	// Kurast temple its own instead of "Stairs" (a tester, 2026-10-26).
	if (openWarp && mode == ObjectOpened) {
		if (const uint32_t levelId = OpenedWarpDestination(room)) {
			entity.name = LevelText(tables, levelId, LevelWarpKey);
			entity.nameKey = LevelKey(tables, levelId, LevelWarpKey);
			entity.destinationLevel = levelId;
		}
		LogUnfolded(entity, classId);
	}
	if (entity.name.empty() && !key.empty()) {
		entity.name = UiText::Clean(Localization::Text(key));
		entity.nameKey = key;
	}
	// The glowing quest and reward chests, told from the plain ones (a tester, 2026-10-17).
	if (Generated::RadiantObject(classId) && !entity.name.empty()) {
		char text[256]{};
		std::snprintf(text, sizeof text, Strings::Current().radiantObject, entity.name.c_str());
		entity.name = text;
		entity.nameKey.clear(); // composed
	}
	// Spatial cues are derived from the object row, not its translated label.
	// Keep those cues alive while string tables are unavailable or reloading.
	if (entity.name.empty() && entity.kind == EntityRadar::Kind::Exit) entity.name = Strings::Current().exitUnnamed;
	if (entity.name.empty() && (entity.container || entity.questObject || entity.cairnStone)) entity.name = Strings::Current().containerUnnamed;
	if (entity.cairnStone) {
		entity.worldName = entity.name;
		entity.nameKey.clear(); // the spoken name is composed below
		const auto& strings = Strings::Current();
		char text[512]{};
		std::snprintf(text, sizeof text, strings.cairnStoneSymbol, entity.worldName.c_str(),
			strings.cairnSymbols[entity.cairnStone - 1]);
		entity.name = text;
	}
	if (seal) { // the game gives all five one label
		entity.seal = seal;
		entity.worldName = entity.name;
		entity.nameKey.clear();
		char text[512]{};
		std::snprintf(text, sizeof text, Strings::Current().sealNumbered, entity.worldName.c_str(), static_cast<int>(seal));
		entity.name = text;
	}
	return !entity.name.empty();
}
// The stair's row as the game uses it, once per unit: its ExitWalk, Offset and select box, so the
// side a stair is taken from can be matched to its row (the `0x40 a=5 b=<unit>` action line).
void LogWarp(const EntityRadar::Entity& entity, uint32_t warpId, uintptr_t row) {
	{
		std::lock_guard lock(g_warpsMutex);
		if (!g_logContext || !g_warpsLogged.insert(entity.placeId ^ (static_cast<uint64_t>(entity.id) << 40)).second) return;
	}
	std::array<int32_t, 4> select{};
	std::array<int32_t, 2> walk{}, offset{};
	uint8_t direction = 0;
	if (!NativeMemory::Copy(row + LvlWarpSelect, select.data(), sizeof select) || !NativeMemory::Copy(row + LvlWarpExitWalk, walk.data(), sizeof walk)
		|| !NativeMemory::Copy(row + LvlWarpOffset, offset.data(), sizeof offset) || !Peek(row + LvlWarpDirection, direction)) return;
	AsyncLog::InfoF(g_logContext, "[exit] unit=%u warp=%u row=%s to=%u at=%.0f,%.0f walk=%d,%d offset=%d,%d select=%d,%d,%d,%d dir=%c",
		entity.id, warpId, CString(row + LvlWarpName, LvlWarpNameBytes).c_str(), entity.destinationLevel, entity.position.x, entity.position.y,
		walk[0], walk[1], offset[0], offset[1], select[0], select[1], select[2], select[3], direction >= ' ' && direction < 0x7F ? direction : '?');
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
	entity.placeId = NativeLevelExits::Identity(entity.position);
	uintptr_t destination = 0, level = 0;
	uint32_t levelId = 0;
	if (Peek(node + WarpNodeDestination, destination) && destination && Peek(destination + ExtendedLevel, level) && level
		&& Peek(level + LevelId, levelId)) {
		entity.name = LevelText(tables, levelId, LevelWarpKey);
		entity.nameKey = LevelKey(tables, levelId, LevelWarpKey);
		entity.destinationLevel = levelId;
		// Loaded, the warp replaces its preset: it keeps the quest category too (a tester, 2026-10-15).
		uintptr_t here = 0;
		uint32_t from = 0;
		if (Peek(roomEx + ExtendedLevel, here) && here) (void)Peek(here + LevelId, from);
		if (EntityRadar::QuestWay(from, levelId) && TowerQuestRunning()) entity.category = EntityRadar::Category::Quest;
	}
	if (entity.name.empty()) entity.name = Strings::Current().exitUnnamed;
	TombSymbols::Mark(entity, Strings::Current());
	LogWarp(entity, warpId, row);
	return true;
}
}

bool Initialize(const D2RL::PluginContext* context) {
	if (!context) { g_base = 0; g_logContext = nullptr; g_corpsesSupported = g_playersSupported = g_rosterSupported = g_rosterCorpsesSupported = g_socketsSupported = g_unbuiltSupported = g_npcAlertsSupported = g_immunities = g_usableCorpsesSupported = false; g_plate.reset(); { std::lock_guard lock(g_usableMutex); g_usable = {}; } g_playerClass = NoClass; return false; }
	if (g_base && g_base == context->exeBase) return true; // already verified for this image
	g_base = 0;
	g_corpsesSupported = g_playersSupported = g_rosterSupported = g_rosterCorpsesSupported = g_socketsSupported = g_unbuiltSupported = false;
	if (!LayoutCheck::Verify(context, context->exeBase, Checks, "scan", "monsters, objects and exits disabled")) return false;
	g_base = context->exeBase;
	g_logContext = context;
	g_plate.reset();
	if (MonsterPlate::Tables tables; LayoutCheck::Verify(context, context->exeBase, PlateChecks, "plate", "monster ranks and enchantments not spoken")
		&& NativeMemory::Copy(g_base + EnchantmentNames, tables.enchantments.data(), sizeof(tables.enchantments))
		&& NativeMemory::Copy(g_base + ChampionKinds, tables.champions.data(), sizeof(tables.champions))) g_plate = tables;
	g_immunities = LayoutCheck::Verify(context, context->exeBase, ImmunityChecks, "immunities", "monster immunities not spoken");
	AsyncLog::Info(context, "[scan] Verified native monster/object/exit reader.");
	g_corpsesSupported = LayoutCheck::Verify(context, context->exeBase, CorpseChecks, "corpse", "player corpse target disabled");
	if (g_corpsesSupported) AsyncLog::Info(context, "[scan] Verified player corpse ownership reader.");
	g_playersSupported = LayoutCheck::Verify(context, context->exeBase, PlayerChecks, "players", "other players not listed");
	if (g_playersSupported) AsyncLog::Info(context, "[scan] Verified other player name reader.");
	g_rosterSupported = g_playersSupported
		&& LayoutCheck::Verify(context, context->exeBase, RosterChecks, "roster", "distant party members not listed");
	if (g_rosterSupported) AsyncLog::Info(context, "[scan] Verified party roster reader.");
	g_rosterCorpsesSupported = g_rosterSupported
		&& LayoutCheck::Verify(context, context->exeBase, RosterCorpseChecks, "roster-corpses", "distant corpses not placed");
	if (g_rosterCorpsesSupported) AsyncLog::Info(context, "[scan] Verified roster corpse reader.");
	g_socketsSupported = LayoutCheck::Verify(context, context->exeBase, SocketChecks, "sockets", "filled sockets not counted");
	if (g_socketsSupported) AsyncLog::Info(context, "[scan] Verified socketed item reader.");
	constexpr const char* NoUnbuilt = "town NPCs in rooms not built yet not listed";
	g_unbuiltSupported = LayoutCheck::Verify(context, context->exeBase, PresetMapLayout::Checks, "town-npcs", NoUnbuilt)
		&& LayoutCheck::Verify(context, context->exeBase, PresetMapLayout::PendingChecks, "town-npcs", NoUnbuilt);
	if (g_unbuiltSupported) AsyncLog::Info(context, "[scan] Verified unbuilt-room preset reader.");
	g_npcAlertsSupported = LayoutCheck::Verify(context, context->exeBase, NpcAlertChecks, "npc-alerts", "the NPC exclamation mark not read");
	if (g_npcAlertsSupported) AsyncLog::Info(context, "[scan] Verified NPC exclamation mark reader.");
	(void)NativePets::Initialize(context);
	g_usableCorpsesSupported = LayoutCheck::Verify(context, context->exeBase, UsableCorpseChecks, "corpses", "a Necromancer's corpses not counted");
	if (g_usableCorpsesSupported) AsyncLog::Info(context, "[scan] Verified usable corpse reader.");
	g_portalOwnersSupported = LayoutCheck::Verify(context, context->exeBase, PortalOwnerChecks, "portals", "town portals not said with their owner");
	const HMODULE core = GetModuleHandleW(CoreSlots::ModuleName);
	g_widePortalLevel = core && GetProcAddress(core, WidePortalLevelExport);
	AsyncLog::InfoF(context, "[scan] Portal owners %s; portal level high byte %s.", g_portalOwnersSupported ? "verified" : "off",
		g_widePortalLevel ? "from the loader" : "absent");
	return true;
}
std::optional<uint32_t> FilledSockets(uintptr_t unit) {
	const NativeMemory::ReadScope scope;
	uint32_t type = 0, id = 0, magic = 0, ownerId = 0, count = 0;
	uintptr_t inventory = 0, owner = 0;
	if (!g_socketsSupported || !unit || !Peek(unit + UnitType, type) || type != GroundItemLayout::ItemUnitType
		|| !Peek(unit + UnitId, id) || !InventoryOf(unit, inventory)) return std::nullopt;
	if (!inventory) return 0u; // an item that never held anything has no inventory
	if (!Peek(inventory, magic) || magic != InventoryMagic || !Peek(inventory + InventoryOwner, owner) || owner != unit
		|| !Peek(inventory + InventoryLinkId, ownerId) || ownerId != id
		|| !Peek(inventory + InventoryItemCount, count) || count > MaxSockets) return std::nullopt;
	return count;
}
bool ReadPlayer(uintptr_t player, uintptr_t tablesAddress, uint32_t playerId, uint64_t session, std::vector<EntityRadar::Entity>& output, bool distantExits, bool levelBearings, bool includeCorpse, bool includePlayers, uintptr_t roster, bool wholeLevel, bool townNpcs) {
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
	if (uint32_t levelId = 0; level && Peek(level + LevelId, levelId)) {
		std::lock_guard lock(g_gameLevelsMutex);
		g_gameLevels.Note(session, static_cast<int>(levelId));
	}
	for (size_t i = 0; i < count; ++i) {
		if (!adjacent[i] || std::find(rooms.begin(), rooms.end(), adjacent[i]) != rooms.end()) continue;
		if (level && RoomLevel(adjacent[i]) != level) continue;
		rooms.push_back(adjacent[i]);
	}
	// The scanner's read goes on through every loaded room of this level, as the
	// walk for other players does. A room past the ring that cannot be read is
	// passed over; it never fails the ring's read.
	const size_t ring = rooms.size();
	for (size_t i = 0; wholeLevel && i < rooms.size(); ++i) {
		uintptr_t list = 0;
		uint32_t listCount = 0;
		std::array<uintptr_t, MaxRooms> next{};
		if (!Peek(rooms[i] + RoomNeighbors, list) || !Peek(rooms[i] + RoomNeighborCount, listCount) || listCount > MaxRooms
			|| (listCount && !NativeMemory::Copy(list, next.data(), listCount * sizeof(uintptr_t)))) continue;
		for (size_t n = 0; n < listCount && rooms.size() < MaxLoadedRooms; ++n)
			if (next[n] && (!level || RoomLevel(next[n]) == level)
				&& std::find(rooms.begin(), rooms.end(), next[n]) == rooms.end()) rooms.push_back(next[n]);
	}
	std::unordered_set<uintptr_t> visited;
	std::vector<EntityRadar::Entity> entities;
	const auto pierce = PlayerPierce(player);
	std::vector<TownNpcs::Live> liveNpcs;
	std::vector<uint32_t> loadedPlayers; // any mode: the roster must not place a loaded unit again
	std::unordered_set<uint32_t> pets;
	if (const auto all = NativePets::All()) for (const auto& pet : *all) pets.insert(pet.id);
	UsableCorpses usable;
	uint32_t playerClass = 0;
	const bool classRead = Peek(player + UnitClass, playerClass);
	if (classRead) g_playerClass = playerClass;
	const bool readCorpses = g_usableCorpsesSupported && classRead && DynamicPosition(player, room, usable.player);
	usable.necromancer = readCorpses && playerClass == NecromancerClass;
	usable.playerId = playerId;
	usable.readMs = GetTickCount64();
	for (size_t r = 0; r < rooms.size(); ++r) {
		const uintptr_t candidateRoom = rooms[r];
		const bool past = r >= ring;
		uintptr_t unit = 0;
		if (!Peek(candidateRoom + RoomFirstUnit, unit)) { if (past) continue; return false; }
		while (unit) {
			if (visited.size() >= MaxUnits || !visited.insert(unit).second) { if (past) break; return false; }
			uint32_t unitType = 0;
			if (!Peek(unit + UnitType, unitType)) { if (past) break; return false; }
			if (uint32_t unitId = 0; unitType == GroundItemLayout::PlayerType && Peek(unit + UnitId, unitId)) loadedPlayers.push_back(unitId);
			EntityRadar::Entity entity;
			if (unitType == MonsterType) NoteDead(unit);
			if (Radar::Position at; readCorpses && !past && unitType == MonsterType
				&& UsableCorpse(unit, tablesAddress, tables.monsterCount, tables.monsters) && DynamicPosition(unit, candidateRoom, at))
				if (uint32_t corpseId = 0; Peek(unit + UnitId, corpseId)) {
					usable.corpses.push_back(at);
					usable.ids.push_back(corpseId);
				}
			if ((includeCorpse && unitType == GroundItemLayout::PlayerType && ReadPlayerCorpse(unit, candidateRoom, playerId, includePlayers, entity))
				|| (includePlayers && unitType == GroundItemLayout::PlayerType && ReadOtherPlayer(unit, candidateRoom, playerId, entity))
				|| (unitType == MonsterType && ReadMonster(unit, candidateRoom, tables, pierce, pets, entity))
				|| (unitType == ObjectType && ReadObject(unit, candidateRoom, tables, session, entity))
				|| (unitType == WarpType && ReadWarp(unit, candidateRoom, tables, entity))) {
				if (unitType == MonsterType && entity.kind == EntityRadar::Kind::Npc) {
					uint32_t classId = 0;
					if (Peek(unit + UnitClass, classId)) liveNpcs.push_back({entities.size(), classId});
				}
				entity.unit = static_cast<uint8_t>(unitType);
				entities.push_back(std::move(entity));
			}
			if (!Peek(unit + UnitNext, unit)) { if (past) break; return false; }
		}
	}
	// Permanent level transitions may outlive their loaded warp units. Read
	// the existing presets across this level, then merge by exact position.
	std::vector<NativeLevelExits::Exit> exits;
	if (distantExits && NativeLevelExits::Read(room, exits)) for (const auto& exit : exits) {
		EntityRadar::Entity entity{exit.warp + 1, EntityRadar::Kind::Exit, exit.position,
			LevelText(tables, exit.destination, LevelWarpKey), NativeLevelExits::Identity(exit.position),
			false, EntityRadar::Category::Exit, exit.destination};
		entity.nameKey = LevelKey(tables, exit.destination, LevelWarpKey);
		if (entity.name.empty()) entity.name = Strings::Current().exitUnnamed;
		if (EntityRadar::QuestWay(exit.source, exit.destination) && TowerQuestRunning()) entity.category = EntityRadar::Category::Quest;
		TombSymbols::Mark(entity, Strings::Current());
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
			entity.nameKey = LevelKey(tables, crossing.level, LevelNameKey);
			if (entity.name.empty()) entity.name = Strings::Current().exitUnnamed;
			if (std::none_of(entities.begin(), entities.end(), [&](const auto& existing) { return EntityRadar::SameTarget(existing, entity); }))
				entities.push_back(std::move(entity));
		}
	}
	if (townNpcs && levelStruct)
		AddTownNpcs(room, levelStruct, std::vector<uintptr_t>(rooms.begin(), rooms.begin() + ring), tables, liveNpcs, session,
			roster && OthersInGame(roster, playerId), entities);
	if (includePlayers && roster && OthersInGame(roster, playerId)) ReadLoadedPlayers(rooms, level, playerId, loadedPlayers, entities);
	if (uint32_t levelId = 0; includePlayers && roster && level && Peek(level + LevelId, levelId))
		ReadRoster(roster, playerId, levelId, loadedPlayers, entities);
	output = std::move(entities);
	{
		std::lock_guard lock(g_usableMutex);
		g_usable = std::move(usable);
	}
	return true;
}
std::vector<uint32_t> TakeDeadMonsters() {
	std::lock_guard lock(g_deadMutex);
	return std::exchange(g_deadMonsters, {});
}
UsableCorpses LastUsableCorpses() {
	std::lock_guard lock(g_usableMutex);
	return g_usable;
}
std::optional<uint32_t> LastPlayerClass() {
	const uint32_t playerClass = g_playerClass.load();
	return playerClass == NoClass ? std::nullopt : std::optional<uint32_t>(playerClass);
}

bool Read(uint32_t playerId, uint64_t session, std::vector<EntityRadar::Entity>& output, bool distantExits, bool levelBearings, bool wholeLevel, bool townNpcs) {
	uintptr_t player = 0, tables = 0;
	if (!g_base || !NativeGroundItems::RuntimeRoots(player, tables)) { output.clear(); return false; }
	uintptr_t roster = 0;
	if (g_rosterSupported && !Peek(g_base + RosterHead, roster)) roster = 0;
	return ReadPlayer(player, tables, playerId, session, output, distantExits, levelBearings, g_corpsesSupported, g_playersSupported, roster, wholeLevel, townNpcs);
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

namespace {
bool ReadLevelTable(Tables& tables) {
	uintptr_t player = 0, tablesAddress = 0;
	return g_base && NativeGroundItems::RuntimeRoots(player, tablesAddress) && tablesAddress
		&& Peek(tablesAddress + Levels, tables.levels) && tables.levels
		&& Peek(tablesAddress + LevelCount, tables.levelCount) && tables.levelCount <= MaxTableRows;
}
}

std::string LevelName(uint32_t levelId) {
	const NativeMemory::ReadScope scope;
	Tables tables;
	return ReadLevelTable(tables) ? LevelText(tables, levelId, LevelNameKey) : std::string{};
}

std::vector<uint64_t> GoneTownNpcs() {
	std::lock_guard lock(g_townPresetsMutex);
	return g_goneTownNpcs;
}
std::string TownPresets() {
	std::lock_guard lock(g_townPresetsMutex);
	return g_townPresets;
}

std::string KeyText(std::string_view key) {
	return key.empty() ? std::string{} : UiText::Clean(Localization::Text(key));
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
bool ReadRosterCorpses(uintptr_t head, uint32_t playerId, std::vector<CorpseMemory::Owner>& owners) {
	const NativeMemory::ReadScope scope;
	owners.clear();
	const uint16_t party = LocalParty(head, playerId);
	uintptr_t entry = head;
	for (size_t i = 0; entry && i < MaxRosterEntries; ++i) {
		CorpseMemory::Owner owner;
		uint16_t memberParty = NoParty;
		int32_t x = 0, y = 0;
		uintptr_t node = 0;
		if (!Peek(entry + RosterUnitId, owner.id) || !Peek(entry + RosterParty, memberParty) || !Peek(entry + RosterLevel, owner.level)
			|| !Peek(entry + RosterX, x) || !Peek(entry + RosterY, y) || !Peek(entry + RosterLife, owner.life)
			|| !Peek(entry + RosterCorpses, node)) return false;
		owner.name = UiText::Clean(CString(entry + RosterName, PlayerNameBytes));
		owner.partyMember = party != NoParty && memberParty == party && owner.id != playerId;
		owner.located = x > 0 && y > 0 && x <= UINT16_MAX && y <= UINT16_MAX;
		owner.position = {static_cast<double>(x), static_cast<double>(y)};
		for (size_t n = 0; node && n < MaxRosterCorpses; ++n) {
			uint32_t id = 0;
			if (!Peek(node + CorpseNodeId, id) || !Peek(node + CorpseNodeNext, node)) return false;
			if (id) owner.corpses.push_back(id);
		}
		owners.push_back(std::move(owner));
		if (!Peek(entry + RosterNext, entry)) return false;
	}
	return true;
}
bool ReadRosterPortals(uintptr_t head, std::vector<EntityRadar::RosterPortals>& roster) {
	const NativeMemory::ReadScope scope;
	roster.clear();
	uintptr_t entry = head;
	for (size_t i = 0; entry && i < MaxRosterEntries; ++i) {
		EntityRadar::RosterPortals member;
		if (Peek(entry + RosterPortal, member.local) && Peek(entry + RosterRemotePortal, member.remote)) {
			member.owner = UiText::Clean(CString(entry + RosterName, PlayerNameBytes));
			if (!member.owner.empty()) roster.push_back(std::move(member));
		}
		if (!Peek(entry + RosterNext, entry)) break;
	}
	return !roster.empty();
}
bool ReadRosterPortals(std::vector<EntityRadar::RosterPortals>& roster) {
	uintptr_t head = 0;
	roster.clear();
	return g_base && g_portalOwnersSupported && g_rosterSupported && Peek(g_base + RosterHead, head) && head && ReadRosterPortals(head, roster);
}
bool ReadRosterCorpses(uint32_t playerId, std::vector<CorpseMemory::Owner>& owners) {
	uintptr_t head = 0;
	owners.clear();
	return g_base && g_rosterCorpsesSupported && Peek(g_base + RosterHead, head) && head && ReadRosterCorpses(head, playerId, owners);
}
}
