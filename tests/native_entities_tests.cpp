#include "native_entities.h"
#include "entity_layout.h"
#include "ground_item_layout.h"
#include "localization.h"
#include "test_support.h"
#include <array>
#include <cstring>
#include <string>

using namespace D2RAccess;
using namespace EntityLayout;
namespace D2RAccess::Localization {
std::string Text(uint32_t id) {
	switch (id) {
	case 2892: return "Akara";
	case 3013: return "Zombi";
	case DoorClosedText: return "Puerta cerrada";
	case DoorOpenText: return "Puerta abierta";
	case 0x2A39: return "Santuario";
	default: return {};
	}
}
std::string Text(std::string_view key) { return key == "Bank" ? "Alijo" : key == "Casket" ? "Ataúd" : std::string{}; }
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
	Memory<UnitSize> player, npc, enemy, chest, door, shrine;
	Memory<PathSize> playerPath, npcPath, enemyPath, chestPath, doorPath, shrinePath, npcData, enemyData, shrineData, shrineRecord;
	Memory<RoomSize> room, otherRoom;
	Memory<0x1600> tables;
	Memory<MonStatStride * 2> monsters;
	Memory<ObjectStride * 3> objects;
	Memory<64> uniqueName;
	Fixture() {
		player.At<uint32_t>(GroundItemLayout::UnitId, 42); player.At(GroundItemLayout::UnitPath, playerPath.Address());
		playerPath.At<uint32_t>(0, 100u << 16); playerPath.At<uint32_t>(4, 100u << 16); playerPath.At(PathRoom, room.Address());
		tables.At(MonStats, monsters.Address()); tables.At<uint64_t>(MonStatCount, 2);
		tables.At(Objects, objects.Address()); tables.At<uint64_t>(ObjectCount, 3);
		// MonStats row 0: Akara (friendly npc/interact); row 1: zombie (hostile, killable).
		monsters.At<uint16_t>(MonNameId, 2892); monsters.At<uint8_t>(MonAlign, AlignFriendly); monsters.At<uint8_t>(MonFlagsA, FlagNpc | FlagInteract);
		monsters.At<uint16_t>(MonStatStride + MonNameId, 3013); monsters.At<uint8_t>(MonStatStride + MonAlign, AlignHostile);
		monsters.At<uint8_t>(MonStatStride + MonFlagsA, FlagKillable);
		// Objects row 0: stash (selectable in mode 0); row 1: door; row 2: shrine.
		objects.Text(ObjNameKey, "Bank"); objects.At<uint8_t>(ObjSelectable + ObjectNeutral, 1);
		objects.Text(ObjectStride + ObjNameKey, "Door"); objects.At<uint8_t>(ObjectStride + ObjIsDoor, 1);
		objects.At<uint8_t>(ObjectStride + ObjSelectable + ObjectNeutral, 1); objects.At<uint8_t>(ObjectStride + ObjSelectable + ObjectOpened, 1);
		objects.Text(ObjectStride * 2 + ObjNameKey, "Shrine"); objects.At<uint8_t>(ObjectStride * 2 + ObjIsShrine, 1);
		objects.At<uint8_t>(ObjectStride * 2 + ObjSelectable + ObjectNeutral, 1);
		Monster(npc, npcPath, npcData, 0, 7, 103, 100);
		Monster(enemy, enemyPath, enemyData, 1, 8, 90, 90);
		Object(chest, chestPath, 0, 9, 100, 104);
		Object(door, doorPath, 1, 10, 110, 100);
		Object(shrine, shrinePath, 2, 11, 100, 110);
		shrine.At(GroundItemLayout::UnitData, shrineData.Address());
		shrineData.At(ObjLinkRecord, shrineRecord.Address()); shrineRecord.At<uint16_t>(ShrineNameId, 0x2A39);
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
	Check(f.Read(entities) && entities.size() == 5, "npc, enemy and three selectable objects are copied");
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
	f.enemyPath.At(PathRoom, f.otherRoom.Address());
	Check(f.Read(entities) && !f.Find(entities, 8), "a path that does not belong to the walked room is rejected");
	f.enemyPath.At(PathRoom, f.room.Address());
	f.enemy.At<uint32_t>(GroundItemLayout::UnitClass, 2);
	Check(f.Read(entities) && !f.Find(entities, 8), "class beyond the table count is rejected");
	f.enemy.At<uint32_t>(GroundItemLayout::UnitClass, 1);
	Check(f.Read(entities) && entities.size() == 5, "fixture restored");
	Check(!NativeEntities::ReadPlayer(f.player.Address(), f.tables.Address(), 41, 7, entities), "another player id is refused");
	f.enemy.At(GroundItemLayout::UnitNext, f.enemy.Address());
	Check(!f.Read(entities), "a unit list cycle fails instead of looping");
	Passed();
	return 0;
}
