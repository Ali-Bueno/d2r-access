#include "target_sight.h"
#include "test_support.h"

#include <string>
#include <vector>

using namespace D2RAccess;
using namespace TargetSight;
namespace {
constexpr uint16_t Wall = 0x1; // BLOCK_WALK, the walking mask the sonar tests use
constexpr int Size = 60;
// One open room with the player in the middle, as in the scan's own tests.
Radar::World OpenWorld(double x = 30, double y = 30) {
	Radar::World world;
	world.player = {x, y};
	world.walkingMask = Wall;
	world.rooms.push_back({0, 0, Size, Size, std::vector<uint16_t>(Size * Size, 0)});
	return world;
}
void BlockRow(Radar::World& world, int y) {
	for (int x = 0; x < Size; ++x) world.rooms[0].cells[static_cast<size_t>(y * Size + x)] = Wall;
}
EntityRadar::Entity Make(uint32_t id, double x, double y, std::string name,
	EntityRadar::Kind kind = EntityRadar::Kind::Enemy) {
	EntityRadar::Entity entity;
	entity.id = id;
	entity.kind = kind;
	entity.position = {x, y};
	entity.name = std::move(name);
	return entity;
}

void LabelNaming() {
	Check(Names("Zombie. Undead", "Zombie"), "the name leads the game's own label");
	Check(Names("Fallen Shaman. Demon 5 - Revives fallen", "Fallen Shaman"), "a decorated label still names it");
	Check(Names("Blood Raven", "Raven"), "a whole word inside the label counts");
	Check(!Names("Zombie. Undead", "Zomb"), "a partial word is not the name");
	Check(!Names("Skeleton Archer", "keleton"), "a match must start on a boundary");
	Check(!Names("Corpse(mojsior)", "Corp"), "a prefix of a word is not the name");
	Check(Names("Corpse(mojsior)", "Corpse"), "punctuation closes the word");
	Check(!Names("Akara", ""), "an unnamed entity never matches");
	Check(!Names("", "Akara"), "an empty label names nothing");
}

void SightAcrossAWall() {
	auto world = OpenWorld();
	BlockRow(world, 34);
	const std::vector<EntityRadar::Entity> behind{Make(1, 30, 38, "Zombie")};
	Check(Of(world, behind, "Zombie. Undead") == Sight::Hidden, "the only bearer of the name is behind the wall");
	const std::vector<EntityRadar::Entity> both{Make(1, 30, 38, "Zombie"), Make(2, 30, 32, "Zombie")};
	Check(Of(world, both, "Zombie. Undead") == Sight::Visible, "another one of the same name is in sight");
	Check(Of(world, both, "Fallen. Demon") == Sight::Unknown, "a name no entity carries stays unknown");
	Check(Of(world, {}, "Zombie. Undead") == Sight::Unknown, "no units read means no evidence");
	Check(Of(world, behind, "") == Sight::Unknown, "an empty label is never hidden");
}

void ReachAndKinds() {
	auto world = OpenWorld();
	BlockRow(world, 34);
	Settings settings;
	// The second one stands 46 subtiles away, past the room the snapshot covers;
	// the ray to it crosses only unknown cells, which never block.
	const std::vector<EntityRadar::Entity> far{Make(1, 30, 38, "Zombie"), Make(2, 30, -16, "Zombie")};
	Check(Of(world, far, "Zombie. Undead") == Sight::Hidden, "a bearer beyond reach cannot be the one meant");
	settings.reach = 80;
	Check(Of(world, far, "Zombie. Undead", settings) == Sight::Visible, "a longer reach admits the distant one");
	const std::vector<EntityRadar::Entity> item{Make(1, 30, 38, "Gold", EntityRadar::Kind::Item)};
	Check(Of(world, item, "Gold") == Sight::Unknown, "ground items keep their own proximity rules");
	const std::vector<EntityRadar::Entity> exit{Make(1, 30, 38, "Cold Plains", EntityRadar::Kind::Exit)};
	Check(Of(world, exit, "Cold Plains") == Sight::Unknown, "level exits are landmarks walls never hide");
	Settings off;
	off.enabled = false;
	const std::vector<EntityRadar::Entity> behind{Make(1, 30, 38, "Zombie")};
	Check(Of(world, behind, "Zombie. Undead", off) == Sight::Unknown, "the gate can be turned off");
}

void ObjectsAndNpcs() {
	auto world = OpenWorld();
	BlockRow(world, 34);
	const std::vector<EntityRadar::Entity> chest{Make(1, 30, 38, "Chest", EntityRadar::Kind::Object)};
	Check(Of(world, chest, "Chest") == Sight::Hidden, "a world object behind the wall is hidden too");
	const std::vector<EntityRadar::Entity> akara{Make(1, 30, 38, "Akara", EntityRadar::Kind::Npc)};
	Check(Of(world, akara, "Akara") == Sight::Hidden, "so is an NPC");
	// The ray excludes the target's own cell, so something standing in the wall
	// (a door, a chest set into it) is still its own bearer.
	const std::vector<EntityRadar::Entity> door{Make(1, 30, 34, "Door", EntityRadar::Kind::Object)};
	Check(Of(world, door, "Door") == Sight::Visible, "a door in the wall is not hidden by itself");
	auto stone = Make(2, 30, 38, "Piedra de Cairn, gota hacia arriba", EntityRadar::Kind::Object);
	stone.worldName = "Piedra de Cairn";
	Check(Of(world, {stone}, "Piedra de Cairn") == Sight::Hidden,
		"the original HUD name still identifies a Cairn Stone with a symbol behind a wall");
	stone.position = {30, 32};
	Check(Of(world, {stone}, "Piedra de Cairn") == Sight::Visible,
		"the original HUD name still identifies a Cairn Stone with a symbol in sight");
}
void FootprintSight() {
	auto world = OpenWorld();
	world.walkingMask = Wall | Radar::Collision::Object | Radar::Collision::Door;
	auto stone = Make(69, 30, 38, "Piedras de Cairn, gota hacia abajo", EntityRadar::Kind::Object);
	stone.worldName = "Piedras de Cairn";
	stone.footprint = {3, 3};
	world.rooms[0].cells[37 * Size + 30] = Radar::Collision::Object;
	Check(Of(world, {stone}, stone.worldName) == Sight::Visible,
		"a stone's own collision cannot hide the original HUD label");
	world.rooms[0].cells[35 * Size + 30] = Wall;
	Check(Of(world, {stone}, stone.worldName) == Sight::Hidden,
		"a wall outside the footprint still hides the HUD label");
	world.rooms[0].cells[35 * Size + 30] = 0;
	world.rooms[0].cells[37 * Size + 30] |= Radar::Collision::Door;
	Check(Of(world, {stone}, stone.worldName) == Sight::Hidden,
		"a door within the footprint still hides the HUD label");
}
}

// The plate's name gate needs the nearest bearer the player can see, and what it is.
void NearestBearer() {
	auto world = OpenWorld();
	auto boss = Make(3, 30, 38, "Rakanishu"); boss.enemy.boss = true;
	const std::vector entities{Make(1, 30, 36, "Zombi"), Make(2, 30, 33, "Zombi"), boss};
	const auto zombie = Find(world, entities, "Zombi. Muerto viviente");
	Check(zombie.sight == Sight::Visible && zombie.kind == EntityRadar::Kind::Enemy && zombie.distance == 3 && !zombie.important,
		"the nearest bearer of a pack's name is reported with its distance");
	Check(Find(world, entities, "Rakanishu").important, "a superunique counts as important");
	BlockRow(world, 32);
	const auto behind = Find(world, {Make(2, 30, 33, "Zombi"), Make(4, 30, 26, "Zombi")}, "Zombi");
	Check(behind.sight == Sight::Visible && behind.distance == 4, "a nearer bearer behind a wall gives way to one in sight");
	Check(Find(world, {Make(2, 30, 33, "Zombi")}, "Zombi").sight == Sight::Hidden, "only hidden bearers stay hidden");
	Check(Find(world, {}, "Zombi").sight == Sight::Unknown, "no bearer is unknown");
}

int main() {
	NearestBearer();
	LabelNaming();
	SightAcrossAWall();
	ReachAndKinds();
	ObjectsAndNpcs();
	FootprintSight();
	Passed();
	return 0;
}
