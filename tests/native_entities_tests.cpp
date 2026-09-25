#include "native_entities.h"
#include "cairn_stones.h"
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

using namespace D2RAccess;
using namespace EntityLayout;
// local/lng/strings/ui.json id of the PlaceholderNameKey ("dummy") entry.
constexpr uint32_t PlaceholderNameId = 5382;
namespace D2RAccess::Localization {
bool available = true;
std::string Text(uint32_t id) {
	if (!available) return {};
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
	if (!available) return {};
	if (key == PlaceholderNameKey) return "una fuerza maligna";
	if (key == "Bank") return "Alijo";
	if (key == "Casket") return "Ataúd";
	if (key == "Inifuss") return "Árbol de Inifuss";
	if (key == "StoneAlpha" || key == "StoneBeta" || key == "StoneGamma"
		|| key == "StoneDelta" || key == "StoneLambda") return "Piedra de Cairn";
	if (key == "To The Cold Plains") return "A las Llanuras Frías";
	if (key == "Cold Plains") return "Llanuras Frías";
	if (key == "Waypoint") return "Punto de ruta";
	if (key == "Duriel's Lair") return "Guarida de Duriel";
	if (key == "Portal") return "Portal";
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
	Localization::available = false;
	Check(f.Read(entities) && f.Find(entities, 17) && f.Find(entities, 17)->questObject
		&& f.Find(entities, 17)->name == Strings::Current().containerUnnamed,
		"quest cue survives an unavailable game string table");
	Localization::available = true;
	f.casket.At<uint32_t>(GroundItemLayout::UnitMode, ObjectOpened);
	Check(f.Read(entities) && !f.Find(entities, 17), "used Inifuss tree cannot continue emitting a quest cue");
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
int main() {
	PlayerCorpseOwnership();
	RosterCorpseLists();
	OtherPlayersAreListed();
	DistantPartyMembers();
	QuestObjectModes();
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
	for (uint16_t rarity : {1u, 2u, 4u, 8u}) {
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
	Passed();
	return 0;
}
