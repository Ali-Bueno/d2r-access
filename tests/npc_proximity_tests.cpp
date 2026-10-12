#include "npc_proximity.h"
#include "speech_strings.h"
#include "test_support.h"

#include <string>

using namespace D2RAccess;
using namespace NpcProximity;
using Radar::Direction;
namespace {
constexpr uint16_t Wall = 0x1; // BLOCK_WALK, the walking mask used by the sonar tests
constexpr int Size = 60;
// One open room with the player in the middle. Screen up is world (-X,-Y).
Radar::World OpenWorld(double x = 30, double y = 30) {
	Radar::World world;
	world.player = {x, y};
	world.walkingMask = Wall;
	world.rooms.push_back({0, 0, Size, Size, std::vector<uint16_t>(Size * Size, 0)});
	return world;
}
void Block(Radar::World& world, int x, int y) { world.rooms[0].cells[static_cast<size_t>(y * Size + x)] = Wall; }
EntityRadar::Entity Make(uint32_t id, double x, double y, std::string name, EntityRadar::Kind kind = EntityRadar::Kind::Npc) {
	EntityRadar::Entity entity; entity.id = id; entity.kind = kind; entity.position = {x, y}; entity.name = std::move(name); return entity;
}
bool Says(const std::optional<Announcement>& announcement, uint32_t id) { return announcement && announcement->entity.id == id; }

void Directions() {
	const Radar::Position player{30, 30};
	Check(DirectionOf(player, {20, 20}) == Direction::Up, "screen up is the -X,-Y diagonal");
	Check(DirectionOf(player, {40, 40}) == Direction::Down, "screen down is +X,+Y");
	Check(DirectionOf(player, {20, 40}) == Direction::Left, "screen left is -X,+Y");
	Check(DirectionOf(player, {40, 20}) == Direction::Right, "screen right is +X,-Y");
	Check(DirectionOf(player, {29, 20}) == Direction::Up, "just inside the up sector is up");
	Check(DirectionOf(player, {40, 29}) == Direction::Right, "just inside the right sector is right");
	Check(DirectionOf(player, {30, 20}) == Direction::Up, "a diagonal tie goes to the earlier direction");
}

void OnlyFriendlyNpcs() {
	Tracker tracker;
	Settings settings;
	const auto world = OpenWorld();
	const std::vector<EntityRadar::Entity> others{Make(1, 32, 30, "Zombi", EntityRadar::Kind::Enemy),
		Make(2, 31, 30, "Cofre", EntityRadar::Kind::Object), Make(3, 30, 31, "Poción", EntityRadar::Kind::Item)};
	Check(!tracker.Update(world, others, settings), "enemies, objects and items are never announced");
	const auto kashya = tracker.Update(world, {Make(4, 36, 30, "Kashya")}, settings);
	Check(Says(kashya, 4) && kashya->distance == 6 && kashya->direction == Direction::Right, "a friendly NPC in range is announced with its distance and direction");
}

void AnnounceOnceUntilGone() {
	Tracker tracker;
	Settings settings;
	settings.range = 10;
	settings.releaseMargin = 3;
	const std::vector<EntityRadar::Entity> npcs{Make(1, 30, 38, "Akara")};
	Check(Says(tracker.Update(OpenWorld(), npcs, settings), 1), "first poll inside range announces");
	Check(!tracker.Update(OpenWorld(), npcs, settings), "the same poll state repeats nothing");
	Check(!tracker.Update(OpenWorld(30, 34), npcs, settings), "walking closer repeats nothing");
	Check(!tracker.Update(OpenWorld(30, 27), npcs, settings), "one step beyond range keeps the memory (hysteresis)");
	Check(!tracker.Update(OpenWorld(30, 30), npcs, settings), "returning inside range after a short excursion stays silent");
	Check(!tracker.Update(OpenWorld(30, 24), npcs, settings), "beyond range plus margin releases silently");
	Check(Says(tracker.Update(OpenWorld(30, 32), npcs, settings), 1), "coming back into range announces again");
	Check(!tracker.Update(OpenWorld(30, 32), {}, settings), "an NPC that vanished releases silently");
	Check(Says(tracker.Update(OpenWorld(30, 32), npcs, settings), 1), "and announces again when it reappears");
	tracker.Reset();
	Check(Says(tracker.Update(OpenWorld(30, 32), npcs, settings), 1), "a reset forgets every announcement");
}

void NearestFirst() {
	Tracker tracker;
	Settings settings;
	settings.range = 10;
	const std::vector<EntityRadar::Entity> camp{Make(1, 30, 38, "Akara"), Make(2, 35, 30, "Kashya"), Make(3, 30, 22, "Charsi")};
	Check(Says(tracker.Update(OpenWorld(), camp, settings), 2), "the nearest of several NPCs in range speaks first");
	Check(!tracker.Update(OpenWorld(), camp, settings), "the farther ones wait");
	Check(Says(tracker.Update(OpenWorld(30, 34), camp, settings), 1), "the one that becomes nearest speaks next");
	Check(!tracker.Update(OpenWorld(30, 30), camp, settings), "an already announced NPC becoming nearest again is silent");
	Check(Says(tracker.Update(OpenWorld(30, 26), camp, settings), 3), "the last one speaks when it is nearest");
	Check(!tracker.Update(OpenWorld(30, 30), camp, settings), "nothing repeats while all three stay in range");
	Tracker ties;
	Check(Says(ties.Update(OpenWorld(), {Make(9, 30, 36, "Warriv"), Make(5, 36, 30, "Gheed")}, settings), 5), "equal distances tie by lower unit id");
	Tracker far;
	Check(!far.Update(OpenWorld(), {Make(1, 30, 41, "Akara")}, settings), "an NPC beyond range is not announced");
}

void LineOfSightOption() {
	auto world = OpenWorld();
	for (int x = 0; x < Size; ++x) Block(world, x, 34);
	const std::vector<EntityRadar::Entity> npcs{Make(1, 30, 38, "Akara")};
	Settings seen;
	seen.range = 10;
	Tracker b;
	Check(seen.requireLineOfSight, "line of sight is required by default");
	Check(!b.Update(world, npcs, seen), "by default a wall hides an NPC");
	Settings through = seen;
	through.requireLineOfSight = false;
	Tracker a;
	Check(Says(a.Update(world, npcs, through), 1), "with the option off the wall does not hide it");
	world.rooms[0].cells[34 * Size + 30] = 0;
	Check(Says(b.Update(world, npcs, seen), 1), "a doorway on the ray reveals it");
}

void Wording() {
	Strings::Select(Strings::Language::esES);
	const auto& strings = Strings::Current();
	EntityRadar::Settings scan;
	Announcement kashya{Make(1, 42, 30, "Kashya"), 12, Direction::Right};
	Check(Describe(kashya, scan, strings) == "Kashya, 12 pasos, a la derecha", "name, distance and direction");
	Announcement akara{Make(2, 30, 29, "Akara"), 1, Direction::Up};
	Check(Describe(akara, scan, strings) == "Akara, aquí mismo", "an NPC beside the player has no distance or direction");
	akara.distance = 2;
	Check(Describe(akara, scan, strings) == "Akara, 2 pasos, adelante", "one step further keeps the direction");
	Announcement close{Make(3, 30, 30.2, "Charsi"), 0.2, Direction::Down};
	Check(Describe(close, scan, strings) == "Charsi, aquí mismo", "under one step drops the direction");
	Strings::Select(Strings::Language::enUS);
	Check(Describe(kashya, scan, Strings::Current()) == "Kashya, 12 steps, to the right", "English wording");
}
}

int main() {
	Directions();
	OnlyFriendlyNpcs();
	AnnounceOnceUntilGone();
	NearestFirst();
	LineOfSightOption();
	Wording();
	Passed();
	return 0;
}
