#include "native_entities.h"
#include "cairn_stones.h"
#include "tomb_symbols.h"
#include "quest_seals.h"
#include "generated/lootables.h"
#include "entity_layout.h"
#include "level_exit_layout.h"
#include "ground_item_layout.h"
#include "localization.h"
#include "speech_strings.h"
#include "test_support.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace D2RAccess;
using namespace EntityLayout;
// local/lng/strings/ui.json id of the PlaceholderNameKey ("dummy") entry.
constexpr uint32_t PlaceholderNameId = 5382;
namespace D2RAccess::Localization {
bool available = true, english = false; // english: the text language switched in the menus
std::string Text(uint32_t id) {
	if (!available) return {};
	switch (id) {
	case PlaceholderNameId: return english ? "an evil force" : "una fuerza maligna";
	case 2892: return "Akara";
	case 3013: return "Zombi";
	case 3014: return "Hratli";
	case 3015: return "Natalya";
	case 3016: return "Nihlathak";
	case 2869: return "Bishibosh";
	case 11072: return "a0n1:%1 %0"; // strChampionFormatXp, esES
	case 3220: return "[fs]campeona[ms]campeón[fp]campeonas[mp]campeones";
	case DoorClosedText: return "Puerta cerrada";
	case DoorOpenText: return "Puerta abierta";
	case 0x2A39: return "Santuario";
	default: return {};
	}
}
std::string Text(std::string_view key) {
	if (!available) return {};
	if (key == PlaceholderNameKey) return english ? "an evil force" : "una fuerza maligna";
	if (key == "Bank") return "Alijo";
	if (key == "Casket") return "Ataúd";
	if (key == "Inifuss") return "Árbol de Inifuss";
	if (key == "Gibbet") return "Jaula de Cain";
	if (key == "Seal") return "Sello";
	if (key == "sewer stairs") return "Escaleras de la cloaca";
	if (key == "teleport pad") return "Plataforma de teletransporte";
	if (key == "StoneAlpha" || key == "StoneBeta" || key == "StoneGamma"
		|| key == "StoneDelta" || key == "StoneLambda") return "Piedra de Cairn";
	if (key == "To The Cold Plains") return "A las Llanuras Frías";
	if (key == "Cold Plains") return "Llanuras Frías";
	if (key == "Waypoint") return "Punto de ruta";
	if (key == "Duriel's Lair") return "Guarida de Duriel";
	if (key == "Portal") return "Portal";
	if (key == "To Tal Rasha's Tomb") return "To Tal Rasha's Tomb";
	if (key == "Tal Rasha's Tomb") return "Tal Rasha's Tomb";
	if (key == PlayerCorpseNameKey) return "Cadáver";
	return {};
}
}
namespace {
template<size_t N> struct Memory {
	std::array<uint8_t, N> data{};
	uintptr_t Address() const { return reinterpret_cast<uintptr_t>(data.data()); }
	template<class T> void At(size_t offset, T value) { std::memcpy(data.data() + offset, &value, sizeof(value)); }
	void Text(size_t offset, const char* text) { std::memcpy(data.data() + offset, text, std::strlen(text) + 1); }
};
constexpr size_t UnitSize = 0x200, PathSize = 0x100, RoomSize = 0xC0;
struct Fixture {
	Memory<UnitSize> player, npc, enemy, chest, door, shrine, warp, walkWarp, portal, waypoint, lair, casket;
	Memory<PathSize> playerPath, npcPath, enemyPath, chestPath, doorPath, shrinePath, casketPath, npcData, enemyData, shrineData, shrineRecord;
	Memory<PathSize> warpPath, walkWarpPath, portalPath, waypointPath, lairPath, portalData;
	Memory<RoomSize> room, otherRoom;
	Memory<0x100> roomEx, destRoomEx;
	Memory<0x200> destLevel; // includes LevelId at +0x1F8
	Memory<0x40> warpNode, walkNode;
	Memory<0x60 * 2> lvlWarps; // row 0: Id 3 to the Cold Plains; row 1: Id 7, NoInteract (a trapdoor's tile)
	Memory<LevelStride * 3> levels; // row 1: Cold Plains; row 2: Duriel's Lair
	Memory<0x1600> tables;
	Memory<MonStatStride * 60> monsters;
	Memory<ObjectStride * 31> objects;
	Memory<64> uniqueName;
	Fixture() {
		player.At<uint32_t>(GroundItemLayout::UnitId, 42); player.At(GroundItemLayout::UnitPath, playerPath.Address());
		playerPath.At<uint32_t>(0, 100u << 16); playerPath.At<uint32_t>(4, 100u << 16); playerPath.At(PathRoom, room.Address());
		tables.At(MonStats, monsters.Address()); tables.At<uint64_t>(MonStatCount, 60);
		tables.At(Objects, objects.Address()); tables.At<uint64_t>(ObjectCount, 31);
		tables.At(Levels, levels.Address()); tables.At<uint64_t>(LevelCount, 3);
		levels.Text(LevelStride + LevelNameKey, "Cold Plains"); levels.Text(LevelStride + LevelWarpKey, "To The Cold Plains");
		levels.Text(LevelStride * 2 + LevelNameKey, "Duriel's Lair"); levels.Text(LevelStride * 2 + LevelWarpKey, "To Duriel's Lair");
		// MonStats row 0: Akara (friendly npc/interact); row 1: zombie (hostile, killable).
		monsters.At<uint16_t>(MonNameId, 2892); monsters.At<uint8_t>(MonAlign, AlignFriendly); monsters.At<uint8_t>(MonFlagsA, FlagNpc | FlagInteract);
		monsters.At<uint16_t>(MonStatStride + MonNameId, 3013); monsters.At<uint8_t>(MonStatStride + MonAlign, AlignHostile);
		monsters.At<uint8_t>(MonStatStride + MonFlagsA, FlagKillable);
		// Objects row 0: stash (selectable in mode 0); row 1: door; row 2: shrine.
		objects.Text(ObjNameKey, "Bank"); objects.At<uint8_t>(ObjSelectable + ObjectNeutral, 1);
		objects.Text(ObjectStride + ObjNameKey, "Door"); objects.At<uint8_t>(ObjectStride + ObjIsDoor, 1);
		objects.At<uint8_t>(ObjectStride + ObjSelectable + ObjectNeutral, 1); objects.At<uint8_t>(ObjectStride + ObjSelectable + ObjectOpened, 1);
		objects.Text(ObjectStride * 2 + ObjNameKey, "Shrine"); objects.At<uint8_t>(ObjectStride * 2 + ObjSubClass, SubClassShrine);
		objects.At<uint8_t>(ObjectStride * 2 + ObjSelectable + ObjectNeutral, 1);
		// Row 3: town portal (selectable only once opened); row 4: waypoint; row 5: Duriel's portal, named after its level.
		objects.Text(ObjectStride * 3 + ObjNameKey, "Portal"); objects.At<uint8_t>(ObjectStride * 3 + ObjSubClass, SubClassPortal);
		objects.At<uint8_t>(ObjectStride * 3 + ObjSelectable + ObjectOpened, 1);
		objects.Text(ObjectStride * 4 + ObjNameKey, "Waypoint"); objects.At<uint8_t>(ObjectStride * 4 + ObjSubClass, SubClassWaypoint);
		objects.At<uint8_t>(ObjectStride * 4 + ObjSelectable + ObjectNeutral, 1);
		objects.Text(ObjectStride * 5 + ObjNameKey, "Duriel's Lair"); objects.At<uint8_t>(ObjectStride * 5 + ObjSelectable + ObjectNeutral, 1);
		// Row 6: a casket — the container shape, clickable while shut and not once
		// open, on one of the game's own container handlers.
		objects.Text(ObjectStride * 6 + ObjNameKey, "Casket");
		objects.At<uint8_t>(ObjectStride * 6 + ObjSelectable + ObjectNeutral, 1);
		objects.At<uint8_t>(ObjectStride * 6 + ObjOperateFn, ContainerOperateFns.front());
		// Warp nodes of the room: Id 3 leads to level 1, Id 7 is a walk-through tile.
		room.At(RoomExtended, roomEx.Address()); roomEx.At(RoomWarpNodes, warpNode.Address());
		warpNode.At(WarpNodeDestination, destRoomEx.Address()); warpNode.At(WarpNodeNext, walkNode.Address());
		warpNode.At<int32_t>(WarpNodeActive, 1); warpNode.At(WarpNodeRow, lvlWarps.Address());
		walkNode.At(WarpNodeDestination, destRoomEx.Address()); walkNode.At<int32_t>(WarpNodeActive, 1); walkNode.At(WarpNodeRow, lvlWarps.Address() + 0x60);
		lvlWarps.At<uint32_t>(LvlWarpId, 3); lvlWarps.At<uint32_t>(0x60 + LvlWarpId, 7); lvlWarps.At<uint32_t>(0x60 + LvlWarpNoInteract, 1);
		destRoomEx.At(ExtendedLevel, destLevel.Address()); destLevel.At<uint32_t>(LevelId, 1);
		Monster(npc, npcPath, npcData, 0, 7, 103, 100);
		Monster(enemy, enemyPath, enemyData, 1, 8, 90, 90);
		Object(chest, chestPath, 0, 9, 100, 104);
		Object(door, doorPath, 1, 10, 110, 100);
		Object(shrine, shrinePath, 2, 11, 100, 110);
		shrine.At(GroundItemLayout::UnitData, shrineData.Address());
		shrineData.At(ObjLinkRecord, shrineRecord.Address()); shrineRecord.At<uint16_t>(ShrineNameId, 0x2A39);
		Warp(warp, warpPath, 3 | (WarpIdMask + 1), 12, 120, 100); // the game compares the Id as a byte
		Warp(walkWarp, walkWarpPath, 7, 13, 125, 100);
		Object(portal, portalPath, 3, 14, 100, 120); portal.At<uint32_t>(GroundItemLayout::UnitMode, ObjectOpened);
		portal.At(GroundItemLayout::UnitData, portalData.Address()); portalData.At<uint8_t>(ObjPortalLevel, 1);
		Object(waypoint, waypointPath, 4, 15, 130, 100);
		Object(lair, lairPath, 5, 16, 100, 130);
		Object(casket, casketPath, 6, 17, 102, 106);
		lair.At(GroundItemLayout::UnitNext, casket.Address());
		shrine.At(GroundItemLayout::UnitNext, warp.Address()); warp.At(GroundItemLayout::UnitNext, walkWarp.Address());
		walkWarp.At(GroundItemLayout::UnitNext, portal.Address()); portal.At(GroundItemLayout::UnitNext, waypoint.Address());
		waypoint.At(GroundItemLayout::UnitNext, lair.Address());
		room.At(GroundItemLayout::RoomFirstUnit, npc.Address());
		npc.At(GroundItemLayout::UnitNext, enemy.Address()); enemy.At(GroundItemLayout::UnitNext, chest.Address());
		chest.At(GroundItemLayout::UnitNext, door.Address()); door.At(GroundItemLayout::UnitNext, shrine.Address());
	}
	void Monster(Memory<UnitSize>& unit, Memory<PathSize>& path, Memory<PathSize>& data, uint32_t classId, uint32_t id, uint32_t x, uint32_t y) {
		unit.At<uint32_t>(GroundItemLayout::UnitType, MonsterType); unit.At(GroundItemLayout::UnitClass, classId);
		unit.At(GroundItemLayout::UnitId, id); unit.At<uint32_t>(GroundItemLayout::UnitMode, 1);
		unit.At(GroundItemLayout::UnitPath, path.Address()); unit.At(GroundItemLayout::UnitData, data.Address());
		path.At<uint32_t>(0, x << 16); path.At<uint32_t>(4, y << 16); path.At(PathRoom, room.Address());
	}
	void Object(Memory<UnitSize>& unit, Memory<PathSize>& path, uint32_t classId, uint32_t id, int x, int y) {
		unit.At<uint32_t>(GroundItemLayout::UnitType, ObjectType); unit.At(GroundItemLayout::UnitClass, classId);
		unit.At(GroundItemLayout::UnitId, id); unit.At<uint32_t>(GroundItemLayout::UnitMode, ObjectNeutral);
		unit.At(GroundItemLayout::UnitPath, path.Address());
		path.At(0, room.Address()); path.At(GroundItemLayout::StaticX, x); path.At(GroundItemLayout::StaticY, y);
	}
	void Warp(Memory<UnitSize>& unit, Memory<PathSize>& path, uint32_t classId, uint32_t id, int x, int y) {
		unit.At<uint32_t>(GroundItemLayout::UnitType, WarpType); unit.At(GroundItemLayout::UnitClass, classId);
		unit.At(GroundItemLayout::UnitId, id); unit.At(GroundItemLayout::UnitPath, path.Address());
		path.At(0, room.Address()); path.At(GroundItemLayout::StaticX, x); path.At(GroundItemLayout::StaticY, y);
	}
	bool Read(std::vector<EntityRadar::Entity>& result) { return NativeEntities::ReadPlayer(player.Address(), tables.Address(), 42, 7, result); }
	const EntityRadar::Entity* Find(const std::vector<EntityRadar::Entity>& entities, uint32_t id) {
		for (const auto& entity : entities) if (entity.id == id) return &entity;
		return nullptr;
	}
};

// Ordinary town NPCs retain the established moving/unloaded fallback policy;
// durable absence is separately limited to the two proven retiring actors.
void RetiredTownPresence() {
	using namespace LevelExitLayout;
	for (const uint32_t monster : TownNpcs::RetiringClasses) {
		Fixture town;
		Memory<0x200> level;
		Memory<0x100> farEx;
		Memory<RoomSize> farRoom;
		Memory<0x30> marker, duplicate;
		Memory<UnitSize> actual;
		Memory<PathSize> actualPath, actualData;
		std::vector<uint8_t> rows(MonStatStride * 527);
		const auto npcRow = [&](uint32_t id, uint16_t name) {
			std::memcpy(rows.data() + id * MonStatStride + MonNameId, &name, sizeof name);
			rows[id * MonStatStride + MonAlign] = AlignFriendly;
			rows[id * MonStatStride + MonFlagsA] = FlagNpc | FlagInteract;
		};
		npcRow(0, 2892); npcRow(monster, monster == 297 ? 3015 : 3016);
		town.tables.At(MonStats, reinterpret_cast<uintptr_t>(rows.data())); town.tables.At<uint64_t>(MonStatCount, 527);
		town.roomEx.At(ExtendedLevel, level.Address()); level.At(LevelFirstRoom, town.roomEx.Address());
		level.At<uint32_t>(LevelId, monster == 297 ? 75 : 109);
		town.roomEx.At(RoomActive, town.room.Address());
		for (auto* room : {&town.roomEx, &farEx}) {
			room->At(ExtendedLevel, level.Address());
			room->At<int>(RoomTileX, 20); room->At<int>(RoomTileY, 20);
			room->At<int>(RoomTileWidth, 8); room->At<int>(RoomTileHeight, 8);
		}
		farEx.At<int>(RoomTileX, 40); farEx.At(RoomActive, farRoom.Address()); farRoom.At(RoomExtended, farEx.Address());
		marker.At<uint32_t>(PresetType, ObjectType); marker.At<uint32_t>(PresetClass, monster == 297 ? 382 : 461);
		marker.At<int>(PresetX, 20); marker.At<int>(PresetY, 20); town.roomEx.At(RoomPresets, marker.Address());
		NativeEntities::TownPresence presence;
		std::vector<EntityRadar::Entity> entities;
		const auto read = [&](bool whole = true) {
			return NativeEntities::ReadPlayer(town.player.Address(), town.tables.Address(), 42, 7, entities,
				false, false, false, false, 0, whole, true, &presence);
		};
		const uint64_t identity = TownNpcs::Identity(monster);
		Check(read() && presence.complete && presence.player == 42 && presence.session == 7
			&& presence.level == (monster == 297 ? 75u : 109u) && presence.absent == std::vector<uint64_t>{identity},
			"complete immediate-ring observation identifies an absent quest-retired town actor from its exact marker");
		// Missing names are a read limitation, not a disappearance.
		Localization::available = false;
		Check(read() && !presence.complete && presence.absent.empty(), "unavailable candidate localization cannot establish NPC absence");
		Localization::available = true;
		Check(read(false) && !presence.complete && presence.absent.empty(), "a narrow reader never publishes level-wide negative presence");
		// Distant rooms remain resident without their units; they are not the
		// immediate player's room/near list, even though the full pass sees them.
		town.roomEx.At(RoomNext, farEx.Address()); town.roomEx.At<uintptr_t>(RoomPresets, 0); farEx.At(RoomPresets, marker.Address());
		Check(read() && presence.complete && presence.absent.empty(), "a distant unloaded candidate remains unknown instead of absent");
		farEx.At<uintptr_t>(RoomPresets, 0); town.roomEx.At(RoomPresets, marker.Address());
		town.Monster(actual, actualPath, actualData, monster, 777, 220, 120);
		actualPath.At(PathRoom, farRoom.Address()); farRoom.At(GroundItemLayout::RoomFirstUnit, actual.Address());
		Check(read() && presence.complete && presence.absent.empty()
			&& std::any_of(entities.begin(), entities.end(), [identity](const auto& npc) {
				return npc.placeId == identity && npc.id == 777 && npc.position.x == 220 && npc.npcApproaches.size() == 1;
			}), "a moved real NPC anywhere resident overrides origin absence and retains its approach");
		actual.At<uint32_t>(GroundItemLayout::UnitMode, 0);
		Check(read() && presence.complete && presence.absent.empty(), "raw native class presence wins even when the name/liveness reader omits its unit");
		farRoom.At<uintptr_t>(GroundItemLayout::RoomFirstUnit, 1);
		Check(read() && !presence.complete && presence.absent.empty(), "tolerated unreadable distant units invalidate only negative presence evidence");
		farRoom.At<uintptr_t>(GroundItemLayout::RoomFirstUnit, 0);
		marker.At(PresetNext, duplicate.Address()); duplicate.At<uint32_t>(PresetType, ObjectType);
		duplicate.At<uint32_t>(PresetClass, monster == 297 ? 382 : 461); duplicate.At<int>(PresetX, 21); duplicate.At<int>(PresetY, 20);
		Check(read() && !presence.complete && presence.absent.empty(), "ambiguous duplicate retirement markers cannot establish absence");
		marker.At<uintptr_t>(PresetNext, 0); marker.At<int>(PresetX, 999);
		Check(read() && !presence.complete && presence.absent.empty(), "a malformed marker position does not become an empty valid approach set");
		marker.At<int>(PresetX, 20); town.npc.At<uint32_t>(GroundItemLayout::UnitClass, 999);
		Check(read() && !presence.complete && presence.absent.empty(), "an invalid raw monster class makes the presence result unknown");
		town.npc.At<uint32_t>(GroundItemLayout::UnitClass, 0);
		Check(!NativeEntities::ReadPlayer(town.player.Address(), town.tables.Address(), 99, 7, entities,
			false, false, false, false, 0, true, true, &presence) && !presence.complete && !presence.player && presence.absent.empty(),
			"a failed player read clears all previous presence evidence");
	}
}

void TownNpcStandIns() {
	using namespace LevelExitLayout;
	Fixture town;
	Memory<0x200> townLevel;
	Memory<0x100> nearEx, farEx;
	Memory<RoomSize> nearRoom, farRoom;
	Memory<0x30> spawn;
	std::vector<EntityRadar::Entity> entities;
	town.roomEx.At(ExtendedLevel, townLevel.Address()); town.roomEx.At(RoomNext, nearEx.Address()); nearEx.At(RoomNext, farEx.Address());
	townLevel.At<uint32_t>(LevelId, 40); townLevel.At(LevelFirstRoom, town.roomEx.Address());
	int tileX = 20;
	for (auto* logical : {&town.roomEx, &nearEx, &farEx}) {
		logical->At(ExtendedLevel, townLevel.Address());
		logical->At<int>(RoomTileX, tileX); logical->At<int>(RoomTileY, 20);
		logical->At<int>(RoomTileWidth, 8); logical->At<int>(RoomTileHeight, 8);
		tileX += 8;
	}
	nearRoom.At(RoomExtended, nearEx.Address()); farRoom.At(RoomExtended, farEx.Address());
	// The player's room has one neighbour; the far room is loaded one hop further.
	std::array<uintptr_t, 1> aroundPlayer{nearRoom.Address()}, nextHop{farRoom.Address()};
	town.room.At(GroundItemLayout::RoomNeighbors, reinterpret_cast<uintptr_t>(aroundPlayer.data()));
	town.room.At<uint32_t>(GroundItemLayout::RoomNeighborCount, 1);
	nearRoom.At(GroundItemLayout::RoomNeighbors, reinterpret_cast<uintptr_t>(nextHop.data()));
	nearRoom.At<uint32_t>(GroundItemLayout::RoomNeighborCount, 1);
	spawn.At<uint32_t>(PresetType, MonsterType); spawn.At<uint32_t>(PresetClass, 0);
	spawn.At<int>(PresetX, 20); spawn.At<int>(PresetY, 10);
	town.npc.At<uint32_t>(GroundItemLayout::UnitClass, 1); // Akara's unit is not read
	const auto read = [&] { return NativeEntities::ReadPlayer(town.player.Address(), town.tables.Address(), 42, 7,
		entities, false, false, false, false, 0, true, true); };
	const auto standIn = [&]() -> const EntityRadar::Entity* {
		const auto found = std::find_if(entities.begin(), entities.end(), [](const auto& npc) { return npc.placeId == 1; });
		return found == entities.end() ? nullptr : &*found;
	};
	farEx.At(RoomPresets, spawn.Address());
	Check(read() && standIn() && standIn()->position.x == 200 && standIn()->position.y == 110 && standIn()->name == "Akara",
		"a spawn in a loaded room past the player's neighbours stands in for an unread unit");
	farEx.At<uintptr_t>(RoomPresets, 0); nearEx.At(RoomPresets, spawn.Address());
	Check(read() && standIn(), "an ordinary wandering NPC is not permanently retired from nearby origin absence");
	town.npc.At<uint32_t>(GroundItemLayout::UnitClass, 0);
	Check(read() && town.Find(entities, 7) && town.Find(entities, 7)->placeId == 1
		&& std::count_if(entities.begin(), entities.end(), [](const auto& npc) { return npc.placeId == 1; }) == 1,
		"a live unit takes its row's identity and no stand-in is added");
}

void PlayerCorpseOwnership() {
	Fixture f;
	Memory<UnitSize> body;
	Memory<PathSize> path, inventory, ownedInventory, ownerRef;
	body.At<uint32_t>(GroundItemLayout::UnitType, GroundItemLayout::PlayerType);
	body.At<uint32_t>(GroundItemLayout::UnitMode, PlayerCorpseMode);
	body.At<uint32_t>(GroundItemLayout::UnitId, 50);
	body.At(GroundItemLayout::UnitPath, path.Address());
	body.At(UnitInventory, inventory.Address());
	path.At<uint32_t>(0, 105u << 16); path.At<uint32_t>(4, 110u << 16);
	path.At(PathRoom, f.room.Address());
	inventory.At<uint32_t>(0, InventoryMagic);
	inventory.At<uint32_t>(InventoryLinkId, 42);
	f.casket.At(GroundItemLayout::UnitNext, body.Address());
	std::vector<EntityRadar::Entity> entities;
	Check(f.Read(entities), "native scan reads a room containing the player's corpse");
	const auto* own = f.Find(entities, 50);
	Check(own && own->kind == EntityRadar::Kind::Corpse && own->category == EntityRadar::Category::Corpse
		&& own->name == Strings::Current().corpseOwn && !own->container && own->position.x == 105 && own->position.y == 110,
		"the local player's body becomes a named, positioned corpse target without container audio");
	Check(f.Find(entities, 17) && f.Find(entities, 17)->container,
		"the scenery casket remains an ordinary container, never a player corpse");
	const auto corpse = *own;
	f.casket.At<uint32_t>(GroundItemLayout::UnitId, 50);
	Check(f.Read(entities) && std::count_if(entities.begin(), entities.end(), [](const auto& entity) { return entity.id == 50; }) == 2,
		"a player corpse and scenery object with the same numeric unit ID both remain in the scan");
	const auto sameIdObject = std::find_if(entities.begin(), entities.end(), [](const auto& entity) {
		return entity.id == 50 && entity.kind == EntityRadar::Kind::Object;
	});
	Check(sameIdObject != entities.end() && !EntityRadar::SameTarget(corpse, *sameIdObject),
		"the corpse cannot alias a world object in beacon target matching");
	f.casket.At<uint32_t>(GroundItemLayout::UnitId, 17);
	Check(NativeEntities::ReadPlayer(f.player.Address(), f.tables.Address(), 42, 7, entities, false, false, false)
		&& !f.Find(entities, 50) && f.Find(entities, 7) && f.Find(entities, 17),
		"disabling only corpse recognition preserves NPC and ordinary object scanning");
	inventory.At<uint32_t>(InventoryLinkId, 43);
	Check(f.Read(entities) && f.Find(entities, 50) && f.Find(entities, 50)->name == "Cadáver",
		"another player's corpse is listed too, with the game's own word when its name cannot be read");
	Memory<PathSize> corpseData;
	corpseData.Text(PlayerName, "Ñandú");
	body.At(GroundItemLayout::UnitData, corpseData.Address());
	char named[128]{};
	std::snprintf(named, sizeof named, Strings::Current().corpseOf, "Ñandú");
	Check(f.Read(entities) && f.Find(entities, 50) && f.Find(entities, 50)->name == named,
		"another player's corpse is named after the character in its PlayerData");
	Check(NativeEntities::ReadPlayer(f.player.Address(), f.tables.Address(), 42, 7, entities, false, false, true, false)
		&& f.Find(entities, 50)->name == "Cadáver", "without the verified name reader it keeps the game's word");
	body.At<uintptr_t>(GroundItemLayout::UnitData, 0);
	inventory.At<uint32_t>(InventoryLinkId, 0);
	Check(f.Read(entities) && !f.Find(entities, 50), "a body whose inventory names no owner is not a corpse");
	inventory.At<uint32_t>(InventoryLinkId, 50);
	Check(f.Read(entities) && !f.Find(entities, 50), "a dead player's own body, before respawn, is not its corpse");
	inventory.At<uint32_t>(InventoryLinkId, 42);
	body.At<uint32_t>(GroundItemLayout::UnitMode, 1);
	Check(f.Read(entities) && !f.Find(entities, 50), "a living player with the same inventory link is excluded");
	body.At<uint32_t>(GroundItemLayout::UnitMode, PlayerCorpseMode);
	inventory.At<uint32_t>(0, 0);
	Check(f.Read(entities) && !f.Find(entities, 50), "an invalid corpse inventory is excluded");
	inventory.At<uint32_t>(0, InventoryMagic);
	body.At(UnitInventoryRef, ownerRef.Address());
	body.At(UnitOwnedInventory, ownedInventory.Address());
	ownerRef.At<uint32_t>(InventoryRefCount, 1);
	ownedInventory.At<uint32_t>(0, InventoryMagic);
	ownedInventory.At<uint32_t>(InventoryLinkId, 43);
	Check(f.Read(entities) && f.Find(entities, 50) && f.Find(entities, 50)->name != Strings::Current().corpseOwn,
		"a live inventory wrapper takes precedence over the base inventory");
	ownedInventory.At<uint32_t>(InventoryLinkId, 42);
	Check(f.Read(entities) && f.Find(entities, 50), "an owned wrapper can identify the player's corpse");
	ownerRef.At<uint32_t>(InventoryRefCount, 0);
	Check(f.Read(entities) && f.Find(entities, 50), "a retired wrapper falls back to the base inventory");
	f.casket.At<uintptr_t>(GroundItemLayout::UnitNext, 0);
	Check(f.Read(entities) && !f.Find(entities, 50), "corpse pickup or unloading removes the target from a new snapshot");
}
void OtherPlayersAreListed() {
	Fixture f;
	Memory<UnitSize> self, ally;
	Memory<PathSize> selfData, allyPath, allyData;
	auto living = [&](Memory<UnitSize>& unit, uint32_t id, uintptr_t path, uintptr_t data) {
		unit.At<uint32_t>(GroundItemLayout::UnitType, GroundItemLayout::PlayerType);
		unit.At<uint32_t>(GroundItemLayout::UnitMode, 1); // PlrMode.txt NU
		unit.At(GroundItemLayout::UnitId, id);
		unit.At(GroundItemLayout::UnitPath, path);
		unit.At(GroundItemLayout::UnitData, data);
	};
	living(self, 42, f.playerPath.Address(), selfData.Address());
	living(ally, 60, allyPath.Address(), allyData.Address());
	selfData.Text(PlayerName, "Yo");
	allyData.Text(PlayerName, "Ñandú");
	allyPath.At<uint32_t>(0, 108u << 16); allyPath.At<uint32_t>(4, 96u << 16); allyPath.At(PathRoom, f.room.Address());
	f.casket.At(GroundItemLayout::UnitNext, self.Address());
	self.At(GroundItemLayout::UnitNext, ally.Address());
	std::vector<EntityRadar::Entity> entities;
	Check(f.Read(entities), "native scan reads a room shared with another player");
	const auto* other = f.Find(entities, 60);
	Check(other && other->kind == EntityRadar::Kind::Player && other->category == EntityRadar::Category::Player
		&& other->name == "Ñandú" && other->position.x == 108 && other->position.y == 96,
		"another living player is listed by the UTF-8 character name in its PlayerData");
	Check(!f.Find(entities, 42), "the local player never lists itself");
	f.casket.At<uint32_t>(GroundItemLayout::UnitId, 60);
	Check(f.Read(entities), "native scan reads a player and an object sharing a unit ID");
	const auto ofKind = [&](EntityRadar::Kind kind) {
		return std::find_if(entities.begin(), entities.end(), [kind](const auto& entity) { return entity.id == 60 && entity.kind == kind; });
	};
	const auto player = ofKind(EntityRadar::Kind::Player), object = ofKind(EntityRadar::Kind::Object);
	Check(player != entities.end() && object != entities.end() && !EntityRadar::SameTarget(*player, *object),
		"a player cannot alias a world object with the same numeric unit ID");
	f.casket.At<uint32_t>(GroundItemLayout::UnitId, 17);
	for (const uint32_t mode : {PlayerDeathMode, PlayerCorpseMode}) {
		ally.At(GroundItemLayout::UnitMode, mode);
		Check(f.Read(entities) && !f.Find(entities, 60), "a dying or dead player is not listed");
	}
	ally.At<uint32_t>(GroundItemLayout::UnitMode, 1);
	ally.At(UnitFlags1, ForcedDead);
	Check(f.Read(entities) && !f.Find(entities, 60), "a player the liveness flag marks dead is not listed");
	ally.At<uint32_t>(UnitFlags1, 0);
	std::memset(allyData.data.data(), 'a', PlayerNameBytes);
	Check(f.Read(entities) && !f.Find(entities, 60), "a name with no terminator inside its 0x3D-byte buffer is rejected");
	allyData.At<uint8_t>(0, 0);
	Check(f.Read(entities) && !f.Find(entities, 60), "an empty name is not listed");
	allyData.Text(PlayerName, "Ñandú");
	Check(NativeEntities::ReadPlayer(f.player.Address(), f.tables.Address(), 42, 7, entities, false, false, true, false)
		&& !f.Find(entities, 60) && f.Find(entities, 7), "disabling only player recognition preserves the rest of the scan");
}
void RosterCorpseLists() {
	// Me, a party member with two corpses on another level, and a stranger with one.
	std::array<Memory<RosterNext + 8>, 3> roster;
	Memory<CorpseNodeNext + 8> first, second, third;
	struct Row { uint32_t id; uint16_t party; uint32_t level; int32_t x, y; uint32_t life; const char* name; uintptr_t corpses; };
	const std::array<Row, 3> rows{{{42, 3, 2, 100, 100, 100, "Yo", 0}, {70, 3, 5, 300, 400, 0, "Lejos", first.Address()},
		{72, NoParty, 2, 0, 0, 0, "Sin grupo", third.Address()}}};
	for (size_t i = 0; i < rows.size(); ++i) {
		roster[i].Text(RosterName, rows[i].name);
		roster[i].At(RosterUnitId, rows[i].id); roster[i].At(RosterParty, rows[i].party); roster[i].At(RosterLevel, rows[i].level);
		roster[i].At(RosterX, rows[i].x); roster[i].At(RosterY, rows[i].y); roster[i].At(RosterLife, rows[i].life);
		roster[i].At(RosterCorpses, rows[i].corpses);
		if (i + 1 < rows.size()) roster[i].At(RosterNext, roster[i + 1].Address());
	}
	first.At<uint32_t>(CorpseNodeId, 90); first.At(CorpseNodeNext, second.Address());
	second.At<uint32_t>(CorpseNodeId, 91);
	third.At<uint32_t>(CorpseNodeId, 92);
	std::vector<CorpseMemory::Owner> owners;
	Check(NativeEntities::ReadRosterCorpses(roster[0].Address(), 42, owners) && owners.size() == 3, "every roster entry is read");
	Check(!owners[0].partyMember && owners[0].corpses.empty() && owners[0].life == 100, "the local player is nobody's party member");
	Check(owners[1].partyMember && owners[1].name == "Lejos" && owners[1].level == 5 && owners[1].located
		&& owners[1].position.x == 300 && owners[1].position.y == 400 && owners[1].life == 0
		&& owners[1].corpses == std::vector<uint32_t>{90, 91}, "a member's level, position, life and corpse ids are read");
	Check(!owners[2].partyMember && !owners[2].located && owners[2].corpses == std::vector<uint32_t>{92},
		"a stranger's corpses are listed without a place");
	second.At(CorpseNodeNext, first.Address());
	Check(NativeEntities::ReadRosterCorpses(roster[0].Address(), 42, owners) && owners[1].corpses.size() == MaxRosterCorpses,
		"a looping corpse list is bounded");
}
void DistantPartyMembers() {
	Fixture f;
	Memory<0x200> here;
	f.roomEx.At(ExtendedLevel, here.Address()); here.At<uint32_t>(LevelId, 2);
	Memory<UnitSize> near;
	Memory<PathSize> nearPath, nearData;
	near.At<uint32_t>(GroundItemLayout::UnitType, GroundItemLayout::PlayerType);
	near.At<uint32_t>(GroundItemLayout::UnitMode, 1);
	near.At<uint32_t>(GroundItemLayout::UnitId, 60);
	near.At(GroundItemLayout::UnitPath, nearPath.Address()); near.At(GroundItemLayout::UnitData, nearData.Address());
	nearPath.At<uint32_t>(0, 108u << 16); nearPath.At<uint32_t>(4, 96u << 16); nearPath.At(PathRoom, f.room.Address());
	nearData.Text(PlayerName, "Cerca");
	f.casket.At(GroundItemLayout::UnitNext, near.Address());
	// Roster: me, the loaded one, a far member, another level, no party, never positioned.
	std::array<Memory<RosterNext + 8>, 6> roster;
	struct Row { uint32_t id; uint16_t party; uint32_t level; int32_t x, y; const char* name; };
	const std::array<Row, 6> rows{{{42, 3, 2, 100, 100, "Yo"}, {60, 3, 2, 500, 500, "Cerca"},
		{70, 3, 2, 300, 400, "Lejos"}, {71, 3, 5, 300, 400, "Otro nivel"},
		{72, NoParty, 2, 300, 400, "Sin grupo"}, {73, 3, 2, 0, 0, "Sin posición"}}};
	for (size_t i = 0; i < rows.size(); ++i) {
		roster[i].Text(RosterName, rows[i].name);
		roster[i].At(RosterUnitId, rows[i].id); roster[i].At(RosterParty, rows[i].party);
		roster[i].At(RosterLevel, rows[i].level); roster[i].At(RosterX, rows[i].x); roster[i].At(RosterY, rows[i].y);
		if (i + 1 < rows.size()) roster[i].At(RosterNext, roster[i + 1].Address());
	}
	auto read = [&](std::vector<EntityRadar::Entity>& entities, uintptr_t head) {
		return NativeEntities::ReadPlayer(f.player.Address(), f.tables.Address(), 42, 7, entities, false, false, true, true, head);
	};
	std::vector<EntityRadar::Entity> entities;
	Check(read(entities, roster[0].Address()), "native scan reads a room with a party roster");
	const auto* far = f.Find(entities, 70);
	Check(far && far->kind == EntityRadar::Kind::Player && far->category == EntityRadar::Category::Player
		&& far->name == "Lejos" && far->position.x == 300 && far->position.y == 400,
		"a party member outside the loaded rooms is placed where the roster says, by name");
	const auto* loaded = f.Find(entities, 60);
	Check(loaded && loaded->position.x == 108 && std::count_if(entities.begin(), entities.end(),
		[](const auto& entity) { return entity.id == 60; }) == 1, "a loaded unit is read from the world, never repeated from the roster");
	Check(!f.Find(entities, 42) && !f.Find(entities, 71) && !f.Find(entities, 72) && !f.Find(entities, 73),
		"the roster adds neither the local player, another level, a non-member nor an unsent position");
	near.At<uint32_t>(GroundItemLayout::UnitMode, PlayerDeathMode);
	Check(read(entities, roster[0].Address()) && !f.Find(entities, 60), "a loaded dying player is not revived from the roster");
	roster[0].At(RosterParty, NoParty);
	Check(read(entities, roster[0].Address()) && !f.Find(entities, 70), "without a party the roster adds nobody");
	roster[0].At<uint16_t>(RosterParty, 3);
	Check(read(entities, 0) && !f.Find(entities, 70), "no roster, no distant players");
}
void PlayersLoadedBeyondTheScanRing() {
	Fixture f;
	Memory<0x200> here;
	f.roomEx.At(ExtendedLevel, here.Address()); here.At<uint32_t>(LevelId, 2);
	// f.room -> next -> far: the ally stands two rooms away, still loaded by the game.
	Memory<RoomSize> next, far;
	Memory<16> nearList, nextList;
	nearList.At(0, next.Address());
	nextList.At(0, f.room.Address()); nextList.At(8, far.Address());
	f.room.At(GroundItemLayout::RoomNeighbors, nearList.Address()); f.room.At<uint32_t>(GroundItemLayout::RoomNeighborCount, 1);
	next.At(GroundItemLayout::RoomNeighbors, nextList.Address()); next.At<uint32_t>(GroundItemLayout::RoomNeighborCount, 2);
	next.At(RoomExtended, f.roomEx.Address()); far.At(RoomExtended, f.roomEx.Address());
	Memory<UnitSize> ally;
	Memory<PathSize> allyPath, allyData;
	ally.At<uint32_t>(GroundItemLayout::UnitType, GroundItemLayout::PlayerType);
	ally.At<uint32_t>(GroundItemLayout::UnitMode, 1);
	ally.At<uint32_t>(GroundItemLayout::UnitId, 60);
	ally.At(GroundItemLayout::UnitPath, allyPath.Address()); ally.At(GroundItemLayout::UnitData, allyData.Address());
	allyPath.At<uint32_t>(0, 140u << 16); allyPath.At<uint32_t>(4, 100u << 16); allyPath.At(PathRoom, far.Address());
	allyData.Text(PlayerName, "Amiga");
	far.At(GroundItemLayout::RoomFirstUnit, ally.Address());
	// The roster still holds where she was when last more than 50 subtiles away.
	std::array<Memory<RosterNext + 8>, 2> roster;
	roster[0].Text(RosterName, "Yo"); roster[0].At<uint32_t>(RosterUnitId, 42); roster[0].At<uint16_t>(RosterParty, 3);
	roster[0].At<uint32_t>(RosterLevel, 2); roster[0].At<int32_t>(RosterX, 100); roster[0].At<int32_t>(RosterY, 100);
	roster[0].At(RosterNext, roster[1].Address());
	roster[1].Text(RosterName, "Amiga"); roster[1].At<uint32_t>(RosterUnitId, 60); roster[1].At<uint16_t>(RosterParty, 3);
	roster[1].At<uint32_t>(RosterLevel, 2); roster[1].At<int32_t>(RosterX, 900); roster[1].At<int32_t>(RosterY, 900);
	auto read = [&](std::vector<EntityRadar::Entity>& entities, uintptr_t head) {
		return NativeEntities::ReadPlayer(f.player.Address(), f.tables.Address(), 42, 7, entities, false, false, true, true, head);
	};
	std::vector<EntityRadar::Entity> entities;
	Check(read(entities, roster[0].Address()), "native scan reads rooms beyond the ring in a game with others");
	const auto* found = f.Find(entities, 60);
	Check(found && found->name == "Amiga" && found->position.x == 140 && found->position.y == 100
		&& std::count_if(entities.begin(), entities.end(), [](const auto& entity) { return entity.id == 60; }) == 1,
		"a party member loaded beyond the scan ring is placed at her unit, not at the stale roster point");
	ally.At<uint32_t>(GroundItemLayout::UnitMode, PlayerDeathMode);
	Check(read(entities, roster[0].Address()) && !f.Find(entities, 60), "a dying player beyond the ring is not revived from the roster");
	ally.At<uint32_t>(GroundItemLayout::UnitMode, 1);
	Memory<0x100> otherEx;
	Memory<0x200> otherLevel;
	otherEx.At(ExtendedLevel, otherLevel.Address()); otherLevel.At<uint32_t>(LevelId, 5);
	next.At(RoomExtended, otherEx.Address());
	Check(read(entities, roster[0].Address()) && f.Find(entities, 60) && f.Find(entities, 60)->position.x == 900,
		"the walk stays on this level; an unreached member falls back to the roster");
	next.At(RoomExtended, f.roomEx.Address());
	Check(read(entities, 0) && !f.Find(entities, 60), "without other players in the roster the wider walk is skipped");
}
void QuestObjectModes() {
	Fixture f;
	constexpr size_t tree = ObjectStride * 30;
	f.objects.Text(tree + ObjNameKey, "Inifuss");
	f.objects.At<uint8_t>(tree + ObjSelectable + ObjectNeutral, 1);
	f.objects.At<uint8_t>(tree + ObjOperateFn, 12);
	f.casket.At<uint32_t>(GroundItemLayout::UnitClass, 30);
	std::vector<EntityRadar::Entity> entities;
	Check(f.Read(entities) && f.Find(entities, 17) && f.Find(entities, 17)->questObject
		&& f.Find(entities, 17)->name == "Árbol de Inifuss",
		"selectable Inifuss tree is a localized quest audio source");
	Check(f.Find(entities, 17)->category == EntityRadar::Category::Quest, "a quest item source is listed with the quest objects");
	Localization::available = false;
	Check(f.Read(entities) && f.Find(entities, 17) && f.Find(entities, 17)->questObject
		&& f.Find(entities, 17)->name == Strings::Current().containerUnnamed,
		"quest cue survives an unavailable game string table");
	Localization::available = true;
	f.casket.At<uint32_t>(GroundItemLayout::UnitMode, ObjectOpened);
	Check(f.Read(entities) && !f.Find(entities, 17), "used Inifuss tree cannot continue emitting a quest cue");
}
// Cain's Gibbet (objects.txt *ID 26): a quest is advanced by using it, and it gives no item.
void QuestMechanismModes() {
	Fixture f;
	constexpr uint32_t gibbet = 26;
	constexpr size_t row = ObjectStride * gibbet;
	f.objects.Text(row + ObjNameKey, "Gibbet");
	f.objects.At<uint8_t>(row + ObjSelectable + ObjectNeutral, 1);
	f.casket.At<uint32_t>(GroundItemLayout::UnitClass, gibbet);
	std::vector<EntityRadar::Entity> entities;
	Check(f.Read(entities) && f.Find(entities, 17) && f.Find(entities, 17)->questObject && !f.Find(entities, 17)->container
		&& f.Find(entities, 17)->category == EntityRadar::Category::Quest && f.Find(entities, 17)->name == "Jaula de Cain",
		"an unused quest mechanism is a named quest object");
	f.objects.At<uint8_t>(row + ObjSelectable + ObjectOpened, 1);
	f.casket.At<uint32_t>(GroundItemLayout::UnitMode, ObjectOpened);
	Check(f.Read(entities) && f.Find(entities, 17) && !f.Find(entities, 17)->questObject
		&& f.Find(entities, 17)->category == EntityRadar::Category::Interactable,
		"one used but still selectable is an ordinary object again");
	f.objects.At<uint8_t>(row + ObjSelectable + ObjectOpened, 0);
	Check(f.Read(entities) && !f.Find(entities, 17), "one the game no longer lets be targeted is gone");
}
// The five Chaos Sanctuary seals (objects.txt DiabloSeal1..5) all carry the label "Seal".
void SealNumbersAndCount() {
	const auto language = Strings::Selected();
	Strings::Select(Strings::Language::enUS);
	Fixture f;
	constexpr uint64_t session = 7; // the fixture's read
	const uint32_t rows = Generated::SealObjects.back() + 1u;
	std::vector<uint8_t> objects(rows * ObjectStride);
	for (const uint32_t classId : Generated::SealObjects) {
		std::memcpy(objects.data() + classId * ObjectStride + ObjNameKey, "Seal", sizeof "Seal");
		objects[classId * ObjectStride + ObjSelectable + ObjectNeutral] = 1;
	}
	f.tables.At(Objects, reinterpret_cast<uintptr_t>(objects.data())); f.tables.At<uint64_t>(ObjectCount, rows);
	f.casket.At<uint32_t>(GroundItemLayout::UnitClass, Generated::SealObjects[2]);
	std::vector<EntityRadar::Entity> entities;
	Check(f.Read(entities) && f.Find(entities, 17) && f.Find(entities, 17)->seal == 3 && f.Find(entities, 17)->name == "Sello 3"
		&& f.Find(entities, 17)->worldName == "Sello" && f.Find(entities, 17)->category == EntityRadar::Category::Quest,
		"a shut seal is a quest object named with its number");
	Check(QuestSeals::Left(session) == QuestSeals::Count && !QuestSeals::TakeOpening(session), "a shut seal leaves all five to open");
	f.casket.At<uint32_t>(GroundItemLayout::UnitMode, ObjectOpened);
	Check(f.Read(entities) && !f.Find(entities, 17), "an opened seal can no longer be targeted");
	const auto opening = QuestSeals::TakeOpening(session);
	Check(opening && *opening == QuestSeals::Count - 1 && QuestSeals::Left(session) == QuestSeals::Count - 1,
		"its opening is read from its mode though it is no longer listed");
	Strings::Select(language);
}
// The Kurast sewer stairs (objects.txt BeneathTheCityStairs): selectable only once the lever opens them.
void SewerStairs() {
	Fixture f;
	constexpr uint32_t plainClass = 29; // a class in none of the generated object sets
	constexpr size_t stairs = ObjectStride * plainClass;
	f.objects.Text(stairs + ObjNameKey, "sewer stairs");
	f.objects.At<uint8_t>(stairs + ObjSelectable + ObjectOpened, 1);
	f.objects.At<uint8_t>(stairs + ObjOperateFn, RoomWarpOperateFn);
	f.casket.At<uint32_t>(GroundItemLayout::UnitClass, plainClass);
	std::vector<EntityRadar::Entity> entities;
	Check(f.Read(entities) && !f.Find(entities, 17), "shut stairs are not offered");
	f.casket.At<uint32_t>(GroundItemLayout::UnitMode, ObjectOpened);
	Check(f.Read(entities) && f.Find(entities, 17) && f.Find(entities, 17)->kind == EntityRadar::Kind::Exit
		&& f.Find(entities, 17)->category == EntityRadar::Category::Exit && f.Find(entities, 17)->name == "Escaleras de la cloaca"
		&& !f.Find(entities, 17)->placeId, "open stairs are an exit, kept to this game since a new one starts them shut");
}
void TeleportPadClass() {
	Fixture f;
	std::vector<EntityRadar::Entity> entities;
	Check(f.Read(entities) && f.Find(entities, 17) && f.Find(entities, 17)->padClass == 0
		&& f.Find(entities, 9) && f.Find(entities, 9)->padClass == 0,
		"an object on any other handler is no teleport pad");
	constexpr size_t pad = ObjectStride * 30;
	f.objects.Text(pad + ObjNameKey, "teleport pad");
	f.objects.At<uint8_t>(pad + ObjSelectable + ObjectNeutral, 1);
	f.objects.At<uint8_t>(pad + ObjOperateFn, TeleportPadOperateFn);
	f.casket.At<uint32_t>(GroundItemLayout::UnitClass, 30);
	Check(f.Read(entities) && f.Find(entities, 17) && f.Find(entities, 17)->padClass == 30
		&& f.Find(entities, 17)->kind == EntityRadar::Kind::Object && !f.Find(entities, 17)->container,
		"a teleport pad keeps its class, the only thing that tells which pad it leads to");
}
void CairnStoneModes() {
	Fixture f;
	constexpr const char* names[] = {"StoneAlpha", "StoneBeta", "StoneGamma", "StoneDelta", "StoneLambda"};
	std::array<Memory<UnitSize>, CairnStones::Count> stones;
	std::array<Memory<PathSize>, CairnStones::Count> paths;
	std::vector<EntityRadar::Entity> entities;
	for (uint32_t classId = 17; classId <= 21; ++classId) {
		const size_t row = ObjectStride * classId;
		f.objects.Text(row + ObjNameKey, names[classId - 17]);
		f.objects.At<uint8_t>(row + ObjOperateFn, CairnStoneOperateFn);
		f.objects.At<uint8_t>(row + ObjSelectable + ObjectNeutral, 1);
		f.objects.At<uint8_t>(row + ObjSelectable + ObjectOpened, 1);
		f.objects.At<uint32_t>(row + ObjSizeX, 3);
		f.objects.At<uint32_t>(row + ObjSizeY, 3);
		f.objects.At<uint8_t>(row + ObjHasCollision + ObjectNeutral, 1);
		f.objects.At<uint8_t>(row + ObjHasCollision + ObjectOpened, 1);
		const size_t index = classId - 17;
		f.Object(stones[index], paths[index], classId, static_cast<uint32_t>(100 + index), 100 + static_cast<int>(index), 100);
		f.casket.At(GroundItemLayout::UnitNext, stones[0].Address());
		if (index) stones[index - 1].At(GroundItemLayout::UnitNext, stones[index].Address());
	}
	Check(f.Read(entities), "all five unactivated stones are readable without a scroll order");
	for (size_t i = 0; i < CairnStones::Count; ++i) {
		const auto* stone = f.Find(entities, static_cast<uint32_t>(100 + i));
		Check(stone && stone->cairnStone == i + 1 && stone->questObject
			&& stone->footprint.width == 3 && stone->footprint.height == 3
			&& stone->worldName == "Piedra de Cairn"
			&& stone->name.find(Strings::Current().cairnSymbols[i]) != std::string::npos,
			"every unactivated stone gets its cue and fixed symbol while preserving the HUD name");
	}
	const size_t firstRow = ObjectStride * CairnStones::FirstClass;
	f.objects.At<uint8_t>(firstRow + ObjHasCollision + ObjectNeutral, 0);
	Check(f.Read(entities) && f.Find(entities, 100) && f.Find(entities, 100)->footprint.width == 0,
		"a mode without collision does not claim an object footprint");
	f.objects.At<uint8_t>(firstRow + ObjHasCollision + ObjectNeutral, 1);
	f.objects.At<uint8_t>(firstRow + ObjIsDoor, 1);
	Check(f.Read(entities) && f.Find(entities, 100) && f.Find(entities, 100)->footprint.width == 0,
		"doors keep the existing point visibility policy for their separate collision bit");
	f.objects.At<uint8_t>(firstRow + ObjIsDoor, 0);
	Localization::available = false;
	Check(f.Read(entities) && f.Find(entities, 102) && f.Find(entities, 102)->questObject
		&& f.Find(entities, 102)->worldName == Strings::Current().containerUnnamed,
		"Cairn cue survives an unavailable game string table");
	Localization::available = true;
	stones[2].At<uint32_t>(GroundItemLayout::UnitMode, ObjectOperating);
	Check(f.Read(entities) && !f.Find(entities, 102) && f.Find(entities, 104) && f.Find(entities, 104)->questObject,
		"unselectable activating mode one stops its cue without muting the others");
	stones[2].At<uint32_t>(GroundItemLayout::UnitMode, ObjectOpened);
	Check(f.Read(entities) && f.Find(entities, 102) && !f.Find(entities, 102)->questObject
		&& f.Find(entities, 102)->name.find(Strings::Current().cairnSymbols[2]) != std::string::npos
		&& f.Find(entities, 104)->questObject,
		"opened mode two keeps the same symbol as a target but has no quest cue");
	stones[2].At<uint32_t>(GroundItemLayout::UnitMode, 3);
	Check(f.Read(entities) && !f.Find(entities, 102) && f.Find(entities, 100)->questObject,
		"unselectable modes never stop other unactivated stones from sounding");
}
}
namespace {
void TombEntranceSymbols() {
	const auto language = Strings::Selected();
	Strings::Select(Strings::Language::enUS);
	Fixture f;
	Memory<LevelStride * 73> levels;
	f.tables.At(Levels, levels.Address()); f.tables.At<uint64_t>(LevelCount, 73);
	std::vector<EntityRadar::Entity> entities;
	const char* labels[] = {"four-pointed star", "square", "crescent", "circle", "double chevron with a diamond", "triangle", "circle above a crescent"};
	for (uint32_t i = 0; i < 7; ++i) {
		const uint32_t level = 66 + i;
		levels.Text(LevelStride * level + LevelWarpKey, "To Tal Rasha's Tomb");
		levels.Text(LevelStride * level + LevelNameKey, "Tal Rasha's Tomb");
		f.destLevel.At(LevelId, level);
		Check(f.Read(entities), "tomb warp snapshot is readable");
		const auto* warp = f.Find(entities, 12);
		Check(warp && warp->name == std::string("To Tal Rasha's Tomb: ") + labels[i]
			&& warp->destinationLevel == level && warp->worldName == "To Tal Rasha's Tomb"
			&& warp->nameKey == "To Tal Rasha's Tomb",
			"each loaded tomb entrance has its matching journal symbol and retains native identity");
		f.portalData.At<uint16_t>(ObjPortalLevel, static_cast<uint16_t>(level));
		Check(f.Read(entities) && f.Find(entities, 14)->name == "Tal Rasha's Tomb",
			"a town portal to a tomb is not given an entrance carving");
	}
	Check(!TombSymbols::Index(65) && !TombSymbols::Index(73), "adjacent levels are not tomb symbols");
	EntityRadar::Entity remembered{0, EntityRadar::Kind::Exit, {10, 20}, "Tal Rasha's Tomb", 1,
		false, EntityRadar::Category::Exit, 69};
	remembered.nameKey = "Tal Rasha's Tomb";
	TombSymbols::Mark(remembered, Strings::Current());
	TombSymbols::Mark(remembered, Strings::Current());
	Check(remembered.name == "Tal Rasha's Tomb: circle", "presets and restored entrances acquire the symbol once");
	Strings::Select(Strings::Language::esES);
	TombSymbols::Mark(remembered, Strings::Current());
	Check(remembered.name == "Tal Rasha's Tomb: círculo" && remembered.nameKey == "Tal Rasha's Tomb",
		"remembered symbol wording changes language while retaining the relocalizable game key");
	Strings::Select(language);
}
}

int main() {
	TombEntranceSymbols();
	PlayerCorpseOwnership();
	RosterCorpseLists();
	OtherPlayersAreListed();
	PlayersLoadedBeyondTheScanRing();
	DistantPartyMembers();
	QuestObjectModes();
	QuestMechanismModes();
	SealNumbersAndCount();
	SewerStairs();
	TeleportPadClass();
	CairnStoneModes();
	Fixture f;
	std::vector<EntityRadar::Entity> entities;
	Check(f.Read(entities) && entities.size() == 10, "npc, enemy, four objects, one exit warp and three exit objects are copied");
	const auto* warp = f.Find(entities, 12);
	Check(warp && warp->kind == EntityRadar::Kind::Exit && warp->name == "A las Llanuras Frías", "a level exit is named after its destination like the hover label");
	Check(warp->position.x == 120 && warp->position.y == 100, "warp position comes from the static path");
	Check(!f.Find(entities, 13), "walk-through warps the game marks NoInteract are left to their object");
	const auto* portal = f.Find(entities, 14);
	Check(portal && portal->kind == EntityRadar::Kind::Exit && portal->name == "Llanuras Frías", "an open town portal is an exit named after its destination level");
	const auto* waypoint = f.Find(entities, 15);
	Check(waypoint && waypoint->kind == EntityRadar::Kind::Exit && waypoint->name == "Punto de ruta", "a waypoint is an exit by its SubClass bit");
	Check(waypoint->placeId == NativeLevelExits::Identity(waypoint->position),
		"a waypoint is identified by where it stands, so a later game can match it");
	Check(!portal->placeId && !f.Find(entities, 11)->placeId, "a portal or a shrine is not a fixed place");
	const auto* lair = f.Find(entities, 16);
	Check(lair && lair->kind == EntityRadar::Kind::Exit && lair->name == "Guarida de Duriel", "an object named after a level is an exit");
	Check(f.Find(entities, 9)->kind == EntityRadar::Kind::Object, "ordinary objects keep their kind");
	f.warpNode.At<int32_t>(WarpNodeActive, 0);
	Check(f.Read(entities) && !f.Find(entities, 12), "a warp the game would not label yet is skipped");
	f.warpNode.At<int32_t>(WarpNodeActive, 1);
	f.destLevel.At<uint32_t>(LevelId, 99);
	Check(f.Read(entities) && f.Find(entities, 12) && f.Find(entities, 12)->name == Strings::Current().exitUnnamed, "an exit without a level name still counts, generically named");
	f.destLevel.At<uint32_t>(LevelId, 1);
	f.portal.At<uint32_t>(GroundItemLayout::UnitMode, ObjectNeutral);
	Check(f.Read(entities) && !f.Find(entities, 14), "a portal that is not yet open is not targetable");
	f.portal.At<uint32_t>(GroundItemLayout::UnitMode, ObjectOpened);
	const auto* akara = f.Find(entities, 7);
	Check(akara && akara->kind == EntityRadar::Kind::Npc && akara->name == "Akara", "friendly npc keeps its localized base name");
	Check(akara->position.x == 103 && akara->position.y == 100, "monster position comes from the dynamic 16.16 path");
	const auto* zombie = f.Find(entities, 8);
	Check(zombie && zombie->kind == EntityRadar::Kind::Enemy && zombie->name == "Zombi", "hostile killable monster is an enemy");
	Check(zombie && !zombie->enemy.important && !zombie->enemy.resurrector && !zombie->enemy.ranged,
		"an ordinary melee enemy has no cue traits");
	f.monsters.At<uint8_t>(MonStatStride + MonFlagsBoss, FlagBoss);
	f.monsters.At<uint8_t>(MonStatStride + MonFlagsRanged, FlagRanged);
	Check(f.Read(entities) && f.Find(entities, 8)->enemy.important && f.Find(entities, 8)->enemy.ranged,
		"class boss and ranged bits classify without localized names");
	f.monsters.At<uint8_t>(MonStatStride + MonFlagsBoss, FlagPrimeEvil);
	Check(f.Read(entities) && f.Find(entities, 8)->enemy.important, "prime evil bit also marks an important enemy");
	f.monsters.At<uint8_t>(MonStatStride + MonFlagsBoss, 0);
	f.monsters.At<uint8_t>(MonStatStride + MonFlagsRanged, 0);
	for (const uint16_t rarity : {uint16_t{1}, uint16_t{2}, uint16_t{4}, uint16_t{8}}) {
		f.enemyData.At<uint16_t>(MonRarityFlags, rarity);
		Check(f.Read(entities) && f.Find(entities, 8)->enemy.important,
			"each native special-monster rarity bit marks an elite");
	}
	f.enemyData.At<uint16_t>(MonRarityFlags, 0x10);
	Check(f.Read(entities) && !f.Find(entities, 8)->enemy.important,
		"a flag outside the native elite mask does not mark an elite");
	f.enemyData.At<uint16_t>(MonRarityFlags, 0);
	f.monsters.At<uint16_t>(MonStatStride * 58 + MonNameId, 3013);
	f.monsters.At<uint8_t>(MonStatStride * 58 + MonAlign, AlignHostile);
	f.monsters.At<uint8_t>(MonStatStride * 58 + MonFlagsA, FlagKillable);
	f.enemy.At<uint32_t>(GroundItemLayout::UnitClass, 58);
	Check(f.Read(entities) && f.Find(entities, 8)->enemy.resurrector,
		"FallenShaman AI class is a resurrector independent of its displayed name");
	f.enemy.At<uint32_t>(GroundItemLayout::UnitClass, 1);
	Check(f.Read(entities) && !f.Find(entities, 7)->enemy.important && !f.Find(entities, 7)->enemy.ranged,
		"a friendly NPC has no enemy cue traits");
	const auto* chest = f.Find(entities, 9);
	Check(chest && chest->kind == EntityRadar::Kind::Object && chest->name == "Alijo", "object name resolves through its objects.txt key");
	Check(chest->position.x == 100 && chest->position.y == 104, "object position comes from the static path");
	Check(f.Find(entities, 10) && f.Find(entities, 10)->name == "Puerta cerrada", "closed door uses the door caption");
	Check(f.Find(entities, 11) && f.Find(entities, 11)->name == "Santuario", "shrine name comes from the linked shrines row");

	f.uniqueName.Text(0, "Zombi el Voraz");
	f.enemyData.At(MonCachedName, f.uniqueName.Address());
	Check(f.Read(entities) && f.Find(entities, 8)->name == "Zombi el Voraz", "the game's cached composed name wins over the base name");
	f.enemyData.At<uintptr_t>(MonCachedName, 0);
	const auto nameIds = [&](uint16_t format, uint16_t first, uint16_t second) {
		f.enemyData.At<uint16_t>(MonNameIds, format); f.enemyData.At<uint16_t>(MonNameIds + 2, first);
		f.enemyData.At<uint16_t>(MonNameIds + 4, second); f.enemyData.At<uint16_t>(MonNameIds + 6, NoText);
	};
	nameIds(2869, NoText, NoText);
	Check(f.Read(entities) && f.Find(entities, 8)->name == "Bishibosh", "a super unique is named by its own string, never cached");
	nameIds(11072, 3220, 3013);
	Check(f.Read(entities) && f.Find(entities, 8)->name == "Zombi campeón", "a champion's name is composed before the game caches it");
	nameIds(NoText, NoText, NoText);
	Check(f.Read(entities) && f.Find(entities, 8)->name == "Zombi", "an ordinary monster keeps its base name");

	const auto* casket = f.Find(entities, 17);
	Check(casket && casket->container && casket->kind == EntityRadar::Kind::Object,
		"a container is flagged for the chest loop and stays an ordinary object for everything else");
	for (uint32_t id : {9u, 10u, 11u, 14u, 15u, 16u})
		Check(f.Find(entities, id) && !f.Find(entities, id)->container,
			"stashes, doors, shrines, portals, waypoints and level gates are not containers");
	f.casket.At<uint32_t>(GroundItemLayout::UnitMode, ObjectOpened);
	Check(f.Read(entities) && !f.Find(entities, 17), "an opened container is dropped, so its loop stops");
	f.casket.At<uint32_t>(GroundItemLayout::UnitMode, ObjectNeutral);
	f.objects.At<uint8_t>(ObjectStride * 6 + ObjOperateFn, 2); // the shrine handler
	Check(f.Read(entities) && f.Find(entities, 17) && !f.Find(entities, 17)->container,
		"the same object on another handler is not a container");
	f.objects.At<uint8_t>(ObjectStride * 6 + ObjOperateFn, ContainerOperateFns.front());

	f.door.At<uint32_t>(GroundItemLayout::UnitMode, ObjectOpened);
	Check(f.Read(entities) && f.Find(entities, 10)->name == "Puerta abierta", "opened door changes caption");
	f.door.At<uint32_t>(GroundItemLayout::UnitMode, 5);
	Check(f.Read(entities) && !f.Find(entities, 10), "an object mode the game marks unselectable is skipped");
	f.door.At<uint32_t>(GroundItemLayout::UnitMode, ObjectNeutral);

	f.enemy.At<uint32_t>(GroundItemLayout::UnitMode, MonsterDead);
	Check(f.Read(entities) && !f.Find(entities, 8), "dead monsters are skipped");
	f.enemy.At<uint32_t>(GroundItemLayout::UnitMode, MonsterDeath);
	Check(f.Read(entities) && !f.Find(entities, 8), "dying monsters are skipped");
	f.enemy.At<uint32_t>(GroundItemLayout::UnitMode, 1);
	f.enemy.At<uint32_t>(UnitFlags1, ForcedDead);
	Check(f.Read(entities) && !f.Find(entities, 8), "forced-dead flag skips the unit");
	f.enemy.At<uint32_t>(UnitFlags1, PlayerMinion);
	Check(f.Read(entities) && !f.Find(entities, 8), "the player's own minions are skipped");
	f.enemy.At<uint32_t>(UnitFlags1, 0);
	f.monsters.At<uint16_t>(MonStatStride + MonNameId, 0);
	Check(f.Read(entities) && !f.Find(entities, 8), "monsters the game shows no name for are skipped");
	f.monsters.At<uint16_t>(MonStatStride + MonNameId, 3013);
	f.monsters.At<uint8_t>(MonStatStride + MonFlagsA, 0);
	Check(f.Read(entities) && !f.Find(entities, 8), "hostile units without the killable flag (hidden helpers) are skipped");
	f.monsters.At<uint8_t>(MonStatStride + MonFlagsA, FlagKillable);
	f.monsters.At<uint16_t>(MonStatStride + MonNameId, PlaceholderNameId);
	Check(f.Read(entities) && !f.Find(entities, 8), "a killable monster named by the shared placeholder is skipped");
	f.monsters.At<uint16_t>(MonStatStride + MonNameId, 3013);
	f.monsters.At<uint16_t>(MonNameId, PlaceholderNameId);
	Check(f.Read(entities) && !f.Find(entities, 7), "a friendly unit named by the shared placeholder is skipped");
	f.monsters.At<uint16_t>(MonNameId, 2892);
	// The text language switched in the menus: the placeholder is read again.
	const auto spoken = Strings::Selected();
	Strings::Select(Strings::Language::esES);
	f.monsters.At<uint16_t>(MonStatStride + MonNameId, PlaceholderNameId);
	Check(f.Read(entities) && !f.Find(entities, 8), "the Spanish placeholder is skipped in Spanish");
	Localization::english = true;
	Strings::Select(Strings::Language::enUS);
	Check(f.Read(entities) && !f.Find(entities, 8), "after a switch to English the English placeholder is skipped");
	Localization::english = false;
	Strings::Select(Strings::Language::esES);
	Check(f.Read(entities) && !f.Find(entities, 8), "and back in Spanish the Spanish one again");
	Strings::Select(spoken);
	f.monsters.At<uint16_t>(MonStatStride + MonNameId, 3013);
	f.enemyPath.At(PathRoom, f.otherRoom.Address());
	Check(f.Read(entities) && !f.Find(entities, 8), "a path that does not belong to the walked room is rejected");
	f.enemyPath.At(PathRoom, f.room.Address());
	f.enemy.At<uint32_t>(GroundItemLayout::UnitClass, 60);
	Check(f.Read(entities) && !f.Find(entities, 8), "class beyond the table count is rejected");
	f.enemy.At<uint32_t>(GroundItemLayout::UnitClass, 1);
	Check(f.Read(entities) && entities.size() == 10, "fixture restored");
	Localization::available = false;
	for (size_t i = 0; i < Strings::LanguageCount; ++i) {
		Strings::Select(static_cast<Strings::Language>(i));
		Check(f.Read(entities), "entities remain readable while localization is unavailable");
		for (uint32_t id : {12u, 14u, 15u, 16u}) {
			const auto* exit = f.Find(entities, id);
			Check(exit && exit->kind == EntityRadar::Kind::Exit && exit->name == Strings::Current().exitUnnamed,
				"classified exits use the selected language's generic name without game localization");
		}
		const auto* silentContainer = f.Find(entities, 17);
		Check(silentContainer && silentContainer->container && silentContainer->name == Strings::Current().containerUnnamed,
			"a classified container uses the selected language's generic name without game localization");
		Check(!f.Find(entities, 9), "an ordinary unnamed object remains filtered");
	}
	Localization::available = true;
	Strings::Select(Strings::Language::enUS);
	Check(!NativeEntities::ReadPlayer(f.player.Address(), f.tables.Address(), 41, 7, entities), "another player id is refused");
	f.enemy.At(GroundItemLayout::UnitNext, f.enemy.Address());
	Check(!f.Read(entities), "a unit list cycle fails instead of looping");
	Fixture distant;
	Memory<0x200> sourceLevel;
	Memory<0x100> farRoom;
	Memory<0x30> preset;
	using namespace LevelExitLayout;
	sourceLevel.At<uint32_t>(LevelId, 9); sourceLevel.At(LevelFirstRoom, distant.roomEx.Address());
	distant.roomEx.At(ExtendedLevel, sourceLevel.Address()); distant.roomEx.At(RoomNext, farRoom.Address());
	farRoom.At(ExtendedLevel, sourceLevel.Address()); farRoom.At(RoomPresets, preset.Address());
	farRoom.At(RoomWarpNodes, distant.warpNode.Address());
	for (auto* room : {&distant.roomEx, &farRoom}) {
		room->At<int>(RoomTileX, 20); room->At<int>(RoomTileY, 20);
		room->At<int>(RoomTileWidth, 8); room->At<int>(RoomTileHeight, 8);
	}
	farRoom.At<int>(RoomTileX, 100); farRoom.At<int>(RoomTileY, 200);
	preset.At<uint32_t>(PresetType, WarpType); preset.At<uint32_t>(PresetClass, 3);
	preset.At<int>(PresetX, 20); preset.At<int>(PresetY, 0);
	Check(distant.Read(entities) && entities.size() == 10, "proximity and loop readers retain only native nearby units");
	Check(NativeEntities::ReadPlayer(distant.player.Address(), distant.tables.Address(), 42, 7, entities, true)
		&& entities.size() == 11 && entities.back().name == "A las Llanuras Frías" && entities.back().placeId,
		"directional scan opts into named distant preset exits");
	farRoom.At<int>(RoomTileX, 20); farRoom.At<int>(RoomTileY, 20);
	Check(NativeEntities::ReadPlayer(distant.player.Address(), distant.tables.Address(), 42, 7, entities, true)
		&& entities.size() == 10, "loaded warp and same-position preset merge into one target");
	// Boundaries with the neighbouring areas are level-wide, so they are a
	// second opt-in of their own: the short-range scan asks for distant warps
	// without them. Without a DRLG the reader reports none either way, which
	// is the fail-closed half of the same rule.
	Check(NativeEntities::ReadPlayer(distant.player.Address(), distant.tables.Address(), 42, 7, entities, true, false)
		&& entities.size() == 10, "distant warps do not bring the level boundaries with them");
	Check(NativeEntities::ReadPlayer(distant.player.Address(), distant.tables.Address(), 42, 7, entities, true, true)
		&& entities.size() == 10, "and asking for boundaries adds none when the DRLG cannot be read");
	// A level passage puts the far side's room in this room's neighbour list.
	Fixture across;
	Memory<0x200> nearLevel, farLevel;
	Memory<RoomSize> foreignRoom;
	Memory<0x100> foreignEx;
	Memory<UnitSize> foreignWarp;
	Memory<PathSize> foreignWarpPath;
	std::array<uintptr_t, 1> neighbors{foreignRoom.Address()};
	across.roomEx.At(ExtendedLevel, nearLevel.Address());
	foreignRoom.At(RoomExtended, foreignEx.Address());
	foreignEx.At(ExtendedLevel, farLevel.Address()); foreignEx.At(RoomWarpNodes, across.warpNode.Address());
	foreignWarp.At<uint32_t>(GroundItemLayout::UnitType, WarpType);
	foreignWarp.At<uint32_t>(GroundItemLayout::UnitClass, 3 | (WarpIdMask + 1));
	foreignWarp.At<uint32_t>(GroundItemLayout::UnitId, 77);
	foreignWarp.At(GroundItemLayout::UnitPath, foreignWarpPath.Address());
	foreignWarpPath.At(0, foreignRoom.Address());
	foreignWarpPath.At<int>(GroundItemLayout::StaticX, 900); foreignWarpPath.At<int>(GroundItemLayout::StaticY, 900);
	foreignRoom.At(GroundItemLayout::RoomFirstUnit, foreignWarp.Address());
	across.room.At(GroundItemLayout::RoomNeighbors, reinterpret_cast<uintptr_t>(neighbors.data()));
	across.room.At<uint32_t>(GroundItemLayout::RoomNeighborCount, 1);
	Check(across.Read(entities) && !across.Find(entities, 77), "a neighbour room belonging to another level contributes no entity");
	foreignEx.At(ExtendedLevel, nearLevel.Address());
	Check(across.Read(entities) && across.Find(entities, 77), "a neighbour room of this level is still walked");
	// Lut Gholein reload: a room and its unique NPC preset can remain loaded
	// while its client unit is absent. The room is no reason to hide the NPC.
	Fixture town;
	Memory<0x200> townLevel;
	Memory<0x100> townEx;
	Memory<RoomSize> townRoom;
	Memory<0x30> townPreset, duplicatePreset;
	town.roomEx.At(ExtendedLevel, townLevel.Address()); town.roomEx.At(RoomNext, townEx.Address());
	townLevel.At<uint32_t>(LevelId, 40); townLevel.At(LevelFirstRoom, town.roomEx.Address());
	townEx.At(ExtendedLevel, townLevel.Address()); townEx.At(RoomPresets, townPreset.Address());
	townEx.At(RoomActive, townRoom.Address()); townRoom.At(RoomExtended, townEx.Address());
	std::array<uintptr_t, 1> townNeighbors{townRoom.Address()};
	town.room.At(GroundItemLayout::RoomNeighbors, reinterpret_cast<uintptr_t>(townNeighbors.data()));
	town.room.At<uint32_t>(GroundItemLayout::RoomNeighborCount, 1);
	for (auto* logical : {&town.roomEx, &townEx}) {
		logical->At<int>(RoomTileX, 20); logical->At<int>(RoomTileY, 20);
		logical->At<int>(RoomTileWidth, 8); logical->At<int>(RoomTileHeight, 8);
	}
	townEx.At<int>(RoomTileX, 100); townEx.At<int>(RoomTileY, 200);
	townPreset.At<uint32_t>(PresetType, MonsterType); townPreset.At<uint32_t>(PresetClass, 0);
	townPreset.At<int>(PresetX, 20); townPreset.At<int>(PresetY, 10);
	const auto readTown = [&] { return NativeEntities::ReadPlayer(town.player.Address(), town.tables.Address(), 42, 7,
		entities, false, false, false, false, 0, true, true); };
	Check(readTown(), "town NPC preset reader accepts the reload scene");
	Check(std::count_if(entities.begin(), entities.end(), [](const auto& npc) { return npc.kind == EntityRadar::Kind::Npc; }) == 1
		&& town.Find(entities, 7)->placeId == 1, "a live NPC wins over its distant preset without a second entry");
	town.npc.At<uint32_t>(GroundItemLayout::UnitClass, 1); // no live unit of the preset's class
	Check(readTown() && std::any_of(entities.begin(), entities.end(), [](const auto& npc) {
		return npc.kind == EntityRadar::Kind::Npc && npc.placeId == 1 && npc.position.x == 520 && npc.position.y == 1010;
	}), "a unique town NPC stays listed from its native preset even in a walked, loaded room with no client unit");
	townLevel.At<uint32_t>(LevelId, 9);
	Check(readTown() && std::none_of(entities.begin(), entities.end(), [](const auto& npc) { return npc.placeId == 1; }),
		"monster presets outside towns never reveal distant NPCs");
	townLevel.At<uint32_t>(LevelId, 40);
	townPreset.At(PresetNext, duplicatePreset.Address());
	duplicatePreset.At<uint32_t>(PresetType, MonsterType); duplicatePreset.At<uint32_t>(PresetClass, 0);
	duplicatePreset.At<int>(PresetX, 21); duplicatePreset.At<int>(PresetY, 10);
	Check(readTown() && std::none_of(entities.begin(), entities.end(), [](const auto& npc) { return npc.placeId == 1; }),
		"repeated guard presets still do not invent one named NPC location");
	town.npc.At<uint32_t>(GroundItemLayout::UnitClass, 0);
	Check(readTown() && town.Find(entities, 7)->placeId == 1
		&& std::count_if(entities.begin(), entities.end(), [](const auto& npc) { return npc.placeId == 1; }) == 1,
		"a real interactive town character keeps one live identity despite repeated alternate spawn presets");
	{
		Fixture moving;
		Memory<0x200> level;
		moving.roomEx.At(ExtendedLevel, level.Address());
		level.At<uint32_t>(LevelId, 75); level.At(LevelFirstRoom, moving.roomEx.Address());
		const auto read = [&](bool whole, bool fallback) {
			return NativeEntities::ReadPlayer(moving.player.Address(), moving.tables.Address(), 42, 7,
				entities, false, false, false, false, 0, whole, fallback);
		};
		Check(read(false, false) && moving.Find(entities, 7)->placeId == 1,
			"a live town character has the same stable identity in local/keyboard reads without a spawn preset");
		moving.npcPath.At<uint16_t>(2, 160); moving.npcPath.At<uint16_t>(6, 125);
		Check(read(true, true) && moving.Find(entities, 7)->placeId == 1 && moving.Find(entities, 7)->position.x == 160,
			"the level-wide scanner follows actual NPC movement independently of spawn position");
		moving.monsters.At<uint8_t>(MonFlagsA, FlagInteract); // an interactive friendly row need not set npc
		Check(read(false, false) && moving.Find(entities, 7)->placeId == 1,
			"a named friendly interactive character is remembered even without the npc flag");
		moving.enemy.At<uint32_t>(GroundItemLayout::UnitClass, 0);
		Check(read(true, true) && moving.Find(entities, 7)->placeId == 0 && moving.Find(entities, 8)->placeId == 0,
			"separate simultaneous units of one class are never collapsed into one town character");
		moving.enemy.At<uint32_t>(GroundItemLayout::UnitClass, 1);
		Memory<RoomSize> remote;
		Memory<0x100> logical;
		Memory<UnitSize> character;
		Memory<PathSize> characterPath, characterData;
		moving.monsters.At<uint16_t>(2 * MonStatStride + MonNameId, 2892);
		moving.monsters.At<uint8_t>(2 * MonStatStride + MonAlign, AlignFriendly);
		moving.monsters.At<uint8_t>(2 * MonStatStride + MonFlagsA, FlagInteract);
		moving.Monster(character, characterPath, characterData, 2, 777, 300, 310);
		characterPath.At(PathRoom, remote.Address());
		remote.At(RoomExtended, logical.Address()); remote.At(GroundItemLayout::RoomFirstUnit, character.Address());
		logical.At(ExtendedLevel, level.Address()); logical.At(RoomActive, remote.Address());
		moving.roomEx.At(RoomNext, logical.Address());
		Check(read(false, false) && !moving.Find(entities, 777), "ordinary proximity does not gain distant units");
		Check(read(true, true) && moving.Find(entities, 777) && moving.Find(entities, 777)->placeId == 3,
			"all active logical rooms are scanned even when missing from the connected active-neighbor ring");
		Memory<0x200> foreign;
		logical.At(ExtendedLevel, foreign.Address());
		Check(read(true, true) && !moving.Find(entities, 777), "active rooms owned by another level remain excluded");
		level.At<uint32_t>(LevelId, 9);
		Check(read(false, false) && moving.Find(entities, 7)->placeId == 0, "non-town NPC identity remains unchanged");
	}
	{
		Fixture questTown;
		Memory<0x200> level;
		Memory<0x30> start, finish, natalya;
		std::vector<uint8_t> rows(MonStatStride * 300);
		const auto row = [&](uint32_t id, uint16_t name) {
			std::memcpy(rows.data() + id * MonStatStride + MonNameId, &name, sizeof name);
			rows[id * MonStatStride + MonAlign] = AlignFriendly;
			rows[id * MonStatStride + MonFlagsA] = FlagNpc | FlagInteract;
		};
		row(0, 2892); row(253, 3014); row(297, 3015);
		questTown.tables.At(MonStats, reinterpret_cast<uintptr_t>(rows.data()));
		questTown.tables.At<uint64_t>(MonStatCount, 300);
		questTown.roomEx.At(ExtendedLevel, level.Address());
		level.At<uint32_t>(LevelId, 75); level.At(LevelFirstRoom, questTown.roomEx.Address());
		questTown.roomEx.At<int>(RoomTileX, 20); questTown.roomEx.At<int>(RoomTileY, 20);
		questTown.roomEx.At<int>(RoomTileWidth, 8); questTown.roomEx.At<int>(RoomTileHeight, 8);
		questTown.roomEx.At(RoomPresets, start.Address());
		start.At<uint32_t>(PresetType, ObjectType); start.At<uint32_t>(PresetClass, 378);
		start.At<int>(PresetX, 5); start.At<int>(PresetY, 5); start.At(PresetNext, finish.Address());
		finish.At<uint32_t>(PresetType, ObjectType); finish.At<uint32_t>(PresetClass, 379);
		finish.At<int>(PresetX, 30); finish.At<int>(PresetY, 30); finish.At(PresetNext, natalya.Address());
		natalya.At<uint32_t>(PresetType, ObjectType); natalya.At<uint32_t>(PresetClass, 382);
		natalya.At<int>(PresetX, 20); natalya.At<int>(PresetY, 20);
		const auto read = [&] { return NativeEntities::ReadPlayer(questTown.player.Address(), questTown.tables.Address(), 42, 7,
			entities, false, false, false, false, 0, true, true); };
		Check(read() && std::any_of(entities.begin(), entities.end(), [](const auto& npc) {
			return npc.name == "Hratli" && npc.placeId == 254 && npc.position.x == 130 && npc.position.y == 130 && npc.unit == EntityRadar::Entity::NoUnit;
		}), "Hratli's quest object end marker supplies one vendor approach, despite no monster preset or client unit");
		Check(std::count_if(entities.begin(), entities.end(), [](const auto& npc) { return npc.name == "Hratli"; }) == 1,
			"Hratli start/end markers are one character, not duplicate NPCs");
		const auto hr = std::find_if(entities.begin(), entities.end(), [](const auto& npc) { return npc.name == "Hratli"; });
		Check(hr != entities.end() && hr->npcApproaches.size() == 2 && hr->npcApproaches.front().x == 130 && hr->npcApproaches.back().x == 105,
			"both quest locations remain available for an unloaded Hratli search, with the vendor end tried first");
		Check(std::any_of(entities.begin(), entities.end(), [](const auto& npc) { return npc.name == "Natalya" && npc.placeId == 298; }),
			"Natalya's object marker is decoded as a character, rather than discarded as dummy decoration");
		questTown.npc.At<uint32_t>(GroundItemLayout::UnitClass, 253);
		Check(read() && questTown.Find(entities, 7)->name == "Hratli" && questTown.Find(entities, 7)->placeId == 254
			&& std::count_if(entities.begin(), entities.end(), [](const auto& npc) { return npc.name == "Hratli"; }) == 1,
			"a moving live Hratli supersedes both quest markers with his actual unit and coordinates");
		Memory<RoomSize> movedRoom;
		Memory<0x100> movedEx;
		movedRoom.At(RoomExtended, movedEx.Address()); movedEx.At(ExtendedLevel, level.Address());
		questTown.npcPath.At(PathRoom, movedRoom.Address());
		Check(read() && questTown.Find(entities, 7)->unit == MonsterType,
			"a town character moving across room lists is read from its current same-level path room");
		level.At<uint32_t>(LevelId, 9);
		Check(read() && std::none_of(entities.begin(), entities.end(), [](const auto& npc) { return npc.name == "Natalya"; }),
			"quest markers in another level never invent Kurast characters");
	}
	TownNpcStandIns();
	RetiredTownPresence();
	Passed();
	return 0;
}
