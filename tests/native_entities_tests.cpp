#include "native_entities.h"
#include "entity_layout.h"
#include "level_exit_layout.h"
#include "ground_item_layout.h"
#include "localization.h"
#include "speech_strings.h"
#include "test_support.h"
#include <array>
#include <cstring>
#include <string>

using namespace D2RAccess;
using namespace EntityLayout;
// local/lng/strings/ui.json id of the PlaceholderNameKey ("dummy") entry.
constexpr uint32_t PlaceholderNameId = 5382;
namespace D2RAccess::Localization {
std::string Text(uint32_t id) {
	switch (id) {
	case PlaceholderNameId: return "una fuerza maligna";
	case 2892: return "Akara";
	case 3013: return "Zombi";
	case DoorClosedText: return "Puerta cerrada";
	case DoorOpenText: return "Puerta abierta";
	case 0x2A39: return "Santuario";
	default: return {};
	}
}
std::string Text(std::string_view key) {
	if (key == PlaceholderNameKey) return "una fuerza maligna";
	if (key == "Bank") return "Alijo";
	if (key == "Casket") return "Ataúd";
	if (key == "To The Cold Plains") return "A las Llanuras Frías";
	if (key == "Cold Plains") return "Llanuras Frías";
	if (key == "Waypoint") return "Punto de ruta";
	if (key == "Duriel's Lair") return "Guarida de Duriel";
	if (key == "Portal") return "Portal";
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
	Memory<UnitSize> player, npc, enemy, chest, door, shrine, warp, walkWarp, portal, waypoint, lair;
	Memory<PathSize> playerPath, npcPath, enemyPath, chestPath, doorPath, shrinePath, npcData, enemyData, shrineData, shrineRecord;
	Memory<PathSize> warpPath, walkWarpPath, portalPath, waypointPath, lairPath, portalData;
	Memory<RoomSize> room, otherRoom;
	Memory<0x100> roomEx, destRoomEx;
	Memory<0x200> destLevel; // includes LevelId at +0x1F8
	Memory<0x40> warpNode, walkNode;
	Memory<0x60 * 2> lvlWarps; // row 0: Id 3 to the Cold Plains; row 1: Id 7, NoInteract (a trapdoor's tile)
	Memory<LevelStride * 3> levels; // row 1: Cold Plains; row 2: Duriel's Lair
	Memory<0x1600> tables;
	Memory<MonStatStride * 2> monsters;
	Memory<ObjectStride * 6> objects;
	Memory<64> uniqueName;
	Fixture() {
		player.At<uint32_t>(GroundItemLayout::UnitId, 42); player.At(GroundItemLayout::UnitPath, playerPath.Address());
		playerPath.At<uint32_t>(0, 100u << 16); playerPath.At<uint32_t>(4, 100u << 16); playerPath.At(PathRoom, room.Address());
		tables.At(MonStats, monsters.Address()); tables.At<uint64_t>(MonStatCount, 2);
		tables.At(Objects, objects.Address()); tables.At<uint64_t>(ObjectCount, 6);
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
}
int main() {
	Fixture f;
	std::vector<EntityRadar::Entity> entities;
	Check(f.Read(entities) && entities.size() == 9, "npc, enemy, three objects, one exit warp and three exit objects are copied");
	const auto* warp = f.Find(entities, 12);
	Check(warp && warp->kind == EntityRadar::Kind::Exit && warp->name == "A las Llanuras Frías", "a level exit is named after its destination like the hover label");
	Check(warp->position.x == 120 && warp->position.y == 100, "warp position comes from the static path");
	Check(!f.Find(entities, 13), "walk-through warps the game marks NoInteract are left to their object");
	const auto* portal = f.Find(entities, 14);
	Check(portal && portal->kind == EntityRadar::Kind::Exit && portal->name == "Llanuras Frías", "an open town portal is an exit named after its destination level");
	const auto* waypoint = f.Find(entities, 15);
	Check(waypoint && waypoint->kind == EntityRadar::Kind::Exit && waypoint->name == "Punto de ruta", "a waypoint is an exit by its SubClass bit");
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
	const auto* chest = f.Find(entities, 9);
	Check(chest && chest->kind == EntityRadar::Kind::Object && chest->name == "Alijo", "object name resolves through its objects.txt key");
	Check(chest->position.x == 100 && chest->position.y == 104, "object position comes from the static path");
	Check(f.Find(entities, 10) && f.Find(entities, 10)->name == "Puerta cerrada", "closed door uses the door caption");
	Check(f.Find(entities, 11) && f.Find(entities, 11)->name == "Santuario", "shrine name comes from the linked shrines row");

	f.uniqueName.Text(0, "Zombi el Voraz");
	f.enemyData.At(MonCachedName, f.uniqueName.Address());
	Check(f.Read(entities) && f.Find(entities, 8)->name == "Zombi el Voraz", "the game's cached composed name wins over the base name");
	f.enemyData.At<uintptr_t>(MonCachedName, 0);

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
	f.enemyPath.At(PathRoom, f.otherRoom.Address());
	Check(f.Read(entities) && !f.Find(entities, 8), "a path that does not belong to the walked room is rejected");
	f.enemyPath.At(PathRoom, f.room.Address());
	f.enemy.At<uint32_t>(GroundItemLayout::UnitClass, 2);
	Check(f.Read(entities) && !f.Find(entities, 8), "class beyond the table count is rejected");
	f.enemy.At<uint32_t>(GroundItemLayout::UnitClass, 1);
	Check(f.Read(entities) && entities.size() == 9, "fixture restored");
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
	Check(distant.Read(entities) && entities.size() == 9, "proximity and loop readers retain only native nearby units");
	Check(NativeEntities::ReadPlayer(distant.player.Address(), distant.tables.Address(), 42, 7, entities, true)
		&& entities.size() == 10 && entities.back().name == "A las Llanuras Frías" && entities.back().levelExitId,
		"directional scan opts into named distant preset exits");
	farRoom.At<int>(RoomTileX, 20); farRoom.At<int>(RoomTileY, 20);
	Check(NativeEntities::ReadPlayer(distant.player.Address(), distant.tables.Address(), 42, 7, entities, true)
		&& entities.size() == 9, "loaded warp and same-position preset merge into one target");
	// Boundaries with the neighbouring areas are level-wide, so they are a
	// second opt-in of their own: the short-range scan asks for distant warps
	// without them. Without a DRLG the reader reports none either way, which
	// is the fail-closed half of the same rule.
	Check(NativeEntities::ReadPlayer(distant.player.Address(), distant.tables.Address(), 42, 7, entities, true, false)
		&& entities.size() == 9, "distant warps do not bring the level boundaries with them");
	Check(NativeEntities::ReadPlayer(distant.player.Address(), distant.tables.Address(), 42, 7, entities, true, true)
		&& entities.size() == 9, "and asking for boundaries adds none when the DRLG cannot be read");
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
	Passed();
	return 0;
}
