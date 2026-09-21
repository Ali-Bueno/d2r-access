#include "entity_radar.h"
#include "speech_strings.h"
#include "test_support.h"

#include <string>

using namespace D2RAccess;
using namespace EntityRadar;
using Radar::Direction;
namespace {
constexpr uint16_t Wall = 0x1; // BLOCK_WALK, the walking mask used by the sonar tests
constexpr int Size = 40;
// One open room with the player in the middle. Screen up is world (-X,-Y).
Radar::World OpenWorld() {
	Radar::World world;
	world.player = {20, 20};
	world.walkingMask = Wall;
	world.rooms.push_back({0, 0, Size, Size, std::vector<uint16_t>(Size * Size, 0)});
	return world;
}
void Block(Radar::World& world, int x, int y) { world.rooms[0].cells[static_cast<size_t>(y * Size + x)] = Wall; }
Entity Make(uint32_t id, double x, double y, std::string name = "Poción") {
	Entity entity; entity.id = id; entity.position = {x, y}; entity.name = std::move(name); return entity;
}
Settings Wide() { Settings settings; settings.range = 100; return settings; }

void Sectors() {
	const Radar::Position player{20, 20};
	Check(InSector(player, {10, 10}, Direction::Up), "screen up is the -X,-Y diagonal");
	Check(!InSector(player, {10, 10}, Direction::Down), "the opposite diagonal is not up");
	Check(InSector(player, {30, 30}, Direction::Down), "screen down is the +X,+Y diagonal");
	Check(InSector(player, {10, 30}, Direction::Left), "screen left is -X,+Y");
	Check(InSector(player, {30, 10}, Direction::Right), "screen right is +X,-Y");
	Check(InSector(player, {19, 10}, Direction::Up) && !InSector(player, {19, 10}, Direction::Right), "a target just inside the up sector is not right");
	Check(InSector(player, {20, 10}, Direction::Up) && InSector(player, {20, 10}, Direction::Right), "a target on the diagonal belongs to both neighbours");
	Check(!InSector(player, {20, 10}, Direction::Left) && !InSector(player, {20, 10}, Direction::Down), "and to neither opposite sector");
	for (auto direction : {Direction::Up, Direction::Left, Direction::Right, Direction::Down})
		Check(InSector(player, player, direction), "a target on the player's cell is reachable from every direction");
}

void Visibility() {
	auto world = OpenWorld();
	Check(LineOfSight(world, world.player, {10, 10}), "an open room hides nothing");
	for (int x = 0; x < Size; ++x) Block(world, x, 15);
	Check(!LineOfSight(world, world.player, {10, 10}), "a wall across the ray hides the target");
	Check(LineOfSight(world, world.player, {30, 30}), "the wall does not affect the other side");
	world.rooms[0].cells[15 * Size + 15] = 0;
	Check(LineOfSight(world, world.player, {10, 10}), "a doorway on the ray restores visibility");
	auto own = OpenWorld();
	Block(own, 10, 10);
	Check(LineOfSight(own, own.player, {10, 10}), "the target's own collision cell never hides it");
	Block(own, 20, 20);
	Check(LineOfSight(own, own.player, {10, 10}), "nor does the player's own cell");
	auto actors = OpenWorld();
	constexpr uint16_t Monster = 0x100;
	actors.walkingMask = Wall | Monster;
	actors.rooms[0].cells[15 * Size + 15] = Monster;
	Check(LineOfSight(actors, actors.player, {10, 10}), "another actor on the ray is not a wall");
}

void Scanning() {
	auto world = OpenWorld();
	const std::vector<Entity> entities{Make(1, 14, 14, "Lejos"), Make(2, 17, 17, "Cerca"), Make(3, 30, 30, "Atrás"),
		Make(4, 2, 2, "Fuera"), Make(5, 17, 17, "Empate")};
	Settings settings;
	settings.range = 10;
	auto targets = Scan(world, entities, Direction::Up, settings);
	Check(targets.size() == 3, "range and sector filter the candidates");
	Check(targets[0].entity.id == 2 && targets[1].entity.id == 5 && targets[2].entity.id == 1, "nearest first, ties by id");
	Check(targets[0].distance > 4.2 && targets[0].distance < 4.3, "distance is the straight-line subtile distance");
	for (int x = 0; x < Size; ++x) Block(world, x, 16);
	targets = Scan(world, entities, Direction::Up, settings);
	Check(targets.size() == 2 && targets[0].entity.id == 2 && targets[1].entity.id == 5, "a wall removes what is behind it");
	Check(Scan(world, entities, Direction::Down, Wide()).size() == 1, "the other direction still sees its target");
}

void Cycling() {
	auto world = OpenWorld();
	std::vector<Entity> entities{Make(1, 17, 17, "A"), Make(2, 14, 14, "B"), Make(3, 11, 11, "C")};
	const auto settings = Wide();
	Cycler cycler;
	uint64_t now = 1000;
	auto targets = Scan(world, entities, Direction::Up, settings);
	auto result = cycler.Push(targets, Direction::Up, now, settings);
	Check(result.found && result.index == 0 && result.count == 3 && result.target.entity.id == 1, "first push names the nearest");
	result = cycler.Push(targets, Direction::Up, now += 500, settings);
	Check(result.index == 1 && result.target.entity.id == 2, "second push moves to the next nearest");
	result = cycler.Push(targets, Direction::Up, now += 500, settings);
	Check(result.index == 2 && result.target.entity.id == 3, "third push reaches the farthest");
	result = cycler.Push(targets, Direction::Up, now += 500, settings);
	Check(result.index == 0 && result.target.entity.id == 1, "the cycle wraps around");
	result = cycler.Push(targets, Direction::Up, now += 500, settings);
	Check(result.index == 1, "and continues");
	result = cycler.Push(targets, Direction::Up, now += settings.cycleTimeoutMs + 1, settings);
	Check(result.index == 0, "a push after the timeout starts over");
	cycler.Push(targets, Direction::Up, now += 500, settings);
	const auto down = Scan(world, entities, Direction::Down, settings);
	result = cycler.Push(down, Direction::Down, now += 500, settings);
	Check(!result.found && result.count == 0 && result.direction == Direction::Down, "an empty direction reports nothing");
	result = cycler.Push(targets, Direction::Up, now += 500, settings);
	Check(result.index == 0, "a different direction resets the cycle");
	result = cycler.Push(targets, Direction::Up, now += 500, settings);
	Check(result.target.entity.id == 2, "second entity spoken before the list changes");
	entities.erase(entities.begin());
	targets = Scan(world, entities, Direction::Up, settings);
	result = cycler.Push(targets, Direction::Up, now += 500, settings);
	Check(result.count == 2 && result.target.entity.id == 3, "a rescanned list continues after the last spoken id");
	entities.erase(entities.begin() + 1);
	targets = Scan(world, entities, Direction::Up, settings);
	result = cycler.Push(targets, Direction::Up, now += 500, settings);
	Check(result.index == 0 && result.target.entity.id == 2, "a vanished last target restarts from the nearest");
}

void DistantExits() {
	auto world = OpenWorld();
	Entity stairs{42, Kind::Exit, {28, 28}, "A las Catacumbas", 1234};
	Entity enemy{42, Kind::Enemy, {28, 28}, "Zombi"};
	Entity portal{43, Kind::Exit, {28, 28}, "Portal"};
	for (int x = 0; x < Size; ++x) Block(world, x, 25);
	const auto targets = Scan(world, {stairs, enemy, portal}, Direction::Down, {});
	Check(targets.size() == 1 && targets[0].entity.levelExitId == 1234, "a fixed level exit in range is named through a wall while other targets need sight");
	auto distant = stairs; distant.position = {500, 500};
	Check(Scan(world, {distant}, Direction::Down, {}).empty(), "a level exit beyond the range is left to the long-range catalogue");
	Check(Scan(world, {stairs}, Direction::Up, {}).empty(), "stairs still obey the requested direction");
	auto respawned = stairs; respawned.id = 900;
	Check(SameTarget(stairs, respawned), "a level exit survives unit recreation");
	Check(!SameTarget(stairs, enemy), "a native enemy id cannot alias a level exit");
	auto second = stairs; second.levelExitId = 4567;
	Check(!SameTarget(stairs, second), "separate exits with the same warp class keep distinct identities");
	Cycler cycler;
	(void)cycler.Push({{stairs, 10}, {second, 20}}, Direction::Down, 100, {});
	const auto next = cycler.Push({{respawned, 10}, {second, 20}}, Direction::Down, 200, {});
	Check(next.target.entity.levelExitId == second.levelExitId, "cycling survives a preset becoming a loaded warp unit");
}

void Wording() {
	const auto& es = Strings::For(Strings::Language::esES);
	Settings settings;
	Result result;
	result.direction = Direction::Up;
	Check(Describe(result, settings, es) == "Nada adelante", "nothing ahead");
	result.direction = Direction::Down;
	Check(Describe(result, settings, es) == "Nada atrás", "nothing behind");
	result.direction = Direction::Left;
	Check(Describe(result, settings, es) == "Nada a la izquierda", "nothing left");
	result.found = true;
	result.target = {Make(1, 0, 0, "Akara"), 8.2};
	result.index = 0; result.count = 3;
	Check(Describe(result, settings, es) == "Akara, 8 pasos",
		"the name and how far, with no position in the candidate list");
	result.count = 1;
	Check(Describe(result, settings, es) == "Akara, 8 pasos", "a single target skips the count");
	result.target.distance = 1.2;
	Check(Describe(result, settings, es) == "Akara, 1 paso", "the singular step");
	result.target.distance = 0.4;
	Check(Describe(result, settings, es) == "Akara, aquí mismo", "adjacent targets have no distance");
	settings.stepsPerSubtile = 0.5;
	result.target.distance = 8.2;
	Check(Describe(result, settings, es) == "Akara, 4 pasos", "the spoken unit follows the calibration factor");
	Check(Describe(result, settings, Strings::For(Strings::Language::enUS)) == "Akara, 4 steps", "english wording");
}
}

int main() {
	Sectors();
	Visibility();
	Scanning();
	Cycling();
	DistantExits();
	Wording();
	Passed();
	return 0;
}
