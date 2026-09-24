#include "map_survey.h"
#include "test_support.h"

#include <string>

using namespace D2RAccess;
using namespace MapSurvey;

namespace {

constexpr uint16_t Wall = 0x1; // BLOCK_WALK, the walking mask the sonar tests use
constexpr int      Size = 40;

// One open room with the player in the middle, same convention as the entity
// radar tests: screen up is world (-X,-Y).
Radar::World OpenWorld() {
	Radar::World world;
	world.player      = {20, 20};
	world.walkingMask = Wall;
	world.rooms.push_back({0, 0, Size, Size, std::vector<uint16_t>(Size * Size, 0)});
	return world;
}

void Block(Radar::World& world, int x, int y) {
	world.rooms[0].cells[static_cast<size_t>(y * Size + x)] = Wall;
}

// A wall across the whole room except one gap, so a target directly behind it
// is only reachable the long way round.
void WallWithGap(Radar::World& world, int y, int gapX) {
	for (int x = 0; x < Size; ++x) {
		if (x != gapX) Block(world, x, y);
	}
}

EntityRadar::Entity Make(uint32_t id, double x, double y, EntityRadar::Kind kind = EntityRadar::Kind::Item) {
	EntityRadar::Entity entity;
	entity.id       = id;
	entity.kind     = kind;
	entity.position = {x, y};
	entity.name     = "thing " + std::to_string(id);
	return entity;
}

Navigation::Grid GridFor(const Radar::World& world) { return Navigation::Build(world, Size); }

void StepsGoAroundWalls() {
	Radar::World world = OpenWorld();
	// Player at (20,20); the target sits four cells straight up the world Y
	// axis, with a solid wall between them and the only gap far to the left.
	WallWithGap(world, 18, 4);
	const Navigation::Grid grid = GridFor(world);
	const auto landmarks = Survey(world, grid, {Make(1, 20, 16)});
	Check(landmarks.size() == 1, "one object surveyed");
	const Landmark& only = landmarks.front();
	Check(only.reachable, "a target behind a wall with a gap is still reachable");
	Check(only.straightLine < 5, "its straight line is short");
	Check(only.steps > 10, "but its walking distance goes the long way round, through the gap");
}

void UnreachableIsReportedAsSuch() {
	Radar::World world = OpenWorld();
	// Seal a single cell off completely; nothing can walk to it.
	Block(world, 9, 10);
	Block(world, 11, 10);
	Block(world, 10, 9);
	Block(world, 10, 11);
	Block(world, 9, 9);
	Block(world, 11, 11);
	Block(world, 9, 11);
	Block(world, 11, 9);
	const Navigation::Grid grid = GridFor(world);
	const auto landmarks = Survey(world, grid, {Make(1, 10, 10)});
	Check(landmarks.size() == 1, "the sealed object is still catalogued");
	Check(!landmarks.front().reachable, "an object with no route is reported unreachable, not as a straight line");
	Check(landmarks.front().steps < 0, "and carries no step count");
}

void NearestByStepsNotByStraightLine() {
	Radar::World world = OpenWorld();
	WallWithGap(world, 18, 4);
	const Navigation::Grid grid = GridFor(world);
	// Behind the wall: close in a straight line, far on foot.
	// Below the player: further in a straight line, but an open walk.
	const auto landmarks = Survey(world, grid, {Make(1, 20, 16), Make(2, 20, 28)});
	Check(landmarks.size() == 2, "both objects surveyed");
	Check(landmarks.front().entity.id == 2, "the one that is actually closer on foot is named first");
	Check(landmarks.front().steps < landmarks.back().steps, "ordering follows steps");
}

void ReachableSortsAheadOfUnreachable() {
	Radar::World world = OpenWorld();
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (dx != 0 || dy != 0) Block(world, 10 + dx, 10 + dy);
		}
	}
	const Navigation::Grid grid = GridFor(world);
	const auto landmarks = Survey(world, grid, {Make(1, 10, 10), Make(2, 30, 30)});
	Check(landmarks.front().entity.id == 2, "a reachable object outranks a nearer sealed one");
	Check(!landmarks.back().reachable, "the sealed one is still listed, last");
}

void OutOfViewObjectsAreKept() {
	Radar::World world = OpenWorld();
	const Navigation::Grid grid = GridFor(world);
	Registry registry;
	registry.Observe(1, {Make(1, 30, 30)}, grid);
	Check(registry.Entities().size() == 1, "a seen object is catalogued");

	// The grid covers the whole room here, so an absent object IS provably gone.
	for (int i = 0; i < 3; ++i) registry.Observe(1, {}, grid);
	Check(registry.Entities().empty(), "an object the map can see is gone is dropped after repeated misses");
}

void AbsenceOutsideTheMapIsNotForgetting() {
	Radar::World world = OpenWorld();
	// A grid that knows only a small window around the player leaves the far
	// corner unknown, so absence there proves nothing.
	const Navigation::Grid narrow = Navigation::Build(world, 3);
	Registry registry;
	registry.Observe(1, {Make(1, 35, 35)}, narrow);
	for (int i = 0; i < 10; ++i) registry.Observe(1, {}, narrow);
	Check(registry.Entities().size() == 1,
		"an object outside what the map currently knows is kept: that is what the player still has to walk to");
}

void ChangingLevelClears() {
	Radar::World world = OpenWorld();
	const Navigation::Grid grid = GridFor(world);
	Registry registry;
	registry.Observe(1, {Make(1, 30, 30)}, grid);
	registry.Observe(2, {}, grid);
	Check(registry.Entities().empty(), "a new level starts from nothing");
	Check(registry.Level() == 2, "and records which level it is");
}

void ReobservingRefreshesPosition() {
	Radar::World world = OpenWorld();
	const Navigation::Grid grid = GridFor(world);
	Registry registry;
	registry.Observe(1, {Make(1, 30, 30)}, grid);
	registry.Observe(1, {Make(1, 25, 25)}, grid);
	Check(registry.Entities().size() == 1, "the same object is not duplicated");
	Check(registry.Entities().front().position.x == 25, "its position follows the world");
}

void KindFiltering() {
	Radar::World world = OpenWorld();
	const Navigation::Grid grid = GridFor(world);
	const auto landmarks = Survey(world, grid, {
		Make(1, 22, 22, EntityRadar::Kind::Enemy),
		Make(2, 24, 24, EntityRadar::Kind::Exit),
		Make(3, 26, 26, EntityRadar::Kind::Enemy),
	});
	const auto enemies = OfKind(landmarks, EntityRadar::Kind::Enemy, 5);
	Check(enemies.size() == 2, "only the requested kind comes back");
	Check(enemies.front().entity.id == 1, "nearest first within the kind");
	Check(OfKind(landmarks, EntityRadar::Kind::Enemy, 1).size() == 1, "the count caps the list");
	Check(OfKind(landmarks, EntityRadar::Kind::Npc, 5).empty(), "an absent kind yields nothing");
}

void CategoryCycling() {
	Radar::World world = OpenWorld();
	const Navigation::Grid grid = GridFor(world);
	auto with = [](uint32_t id, double x, double y, EntityRadar::Category category) {
		EntityRadar::Entity entity = Make(id, x, y);
		entity.category = category;
		return entity;
	};
	const auto landmarks = Survey(world, grid, {
		with(1, 22, 22, EntityRadar::Category::Treasure),
		with(2, 30, 30, EntityRadar::Category::Exit),
		with(3, 24, 24, EntityRadar::Category::Treasure),
		with(4, 26, 26, EntityRadar::Category::Shrine),
	});

	const auto present = Categories(landmarks);
	Check(present.size() == 3, "only categories that have something in them are offered");
	Check(present.front() == EntityRadar::Category::Exit, "the way out comes first for a stranded player");

	const auto treasure = OfCategory(landmarks, EntityRadar::Category::Treasure);
	Check(treasure.size() == 2, "a category returns all of its members");
	Check(treasure.front().entity.id == 1, "nearest on foot first inside the category");
	Check(OfCategory(landmarks, EntityRadar::Category::Npc).empty(), "an absent category is empty");

	const auto next = StepCategory(landmarks, EntityRadar::Category::Exit, 1);
	Check(next == EntityRadar::Category::Treasure, "stepping forward skips categories with nothing in them");
	Check(StepCategory(landmarks, EntityRadar::Category::Exit, -1) == EntityRadar::Category::Shrine,
		"stepping back from the first wraps to the last");
	Check(StepCategory(landmarks, EntityRadar::Category::Shrine, 1) == EntityRadar::Category::Exit,
		"stepping forward from the last wraps to the first");
	Check(StepCategory(landmarks, EntityRadar::Category::Npc, 1) == EntityRadar::Category::Exit,
		"stepping from a category that is not present starts at the first");
	Check(StepCategory({}, EntityRadar::Category::Exit, 1) == EntityRadar::Category::Unknown,
		"an empty map offers no category at all");
}

void ExplorationDiscoversOnlyNearbyVisibleTargets() {
	Radar::World world = OpenWorld();
	auto nearExit = Make(1, 22, 20, EntityRadar::Kind::Exit);
	nearExit.levelExitId = 100;
	auto distantExit = Make(2, 38, 20, EntityRadar::Kind::Exit);
	distantExit.levelExitId = 200;
	const auto found = Discoverable(world, {nearExit, distantExit, Make(3, 39, 39)});
	Check(found.size() == 1 && found.front().id == 1,
		"nearby visible exit is discovered, but generated distant exits and same-room objectives are hidden");

	Block(world, 21, 20);
	Check(Discoverable(world, {nearExit}).empty(), "a fixed exit behind a wall is not a new discovery");
	world.rooms[0].cells[20 * Size + 21] = 0;
	world.rooms[0].cells[20 * Size + 22] = 0x20; // native unknown subtile
	Check(Discoverable(world, {nearExit}).empty(), "a nearby generated exit on unknown collision is not discovered");
	world.rooms[0].cells[20 * Size + 22] = 0;
	world.rooms[0].cells[20 * Size + 21] = 0x20;
	Check(Discoverable(world, {nearExit}).empty(), "unknown terrain between player and exit cannot reveal it");
}

void KnownLandmarkSurvivesLossOfSight() {
	Radar::World world = OpenWorld();
	const Navigation::Grid grid = GridFor(world);
	Registry registry;
	const auto fixed = Make(1, 24, 20, EntityRadar::Kind::Object);
	const std::vector<EntityRadar::Entity> firstPresent{fixed};
	registry.Observe(1, Discoverable(world, firstPresent), grid, {}, &firstPresent);
	Check(registry.Entities().size() == 1, "visible landmark enters the catalogue");
	Block(world, 22, 20);
	const auto hidden = Make(1, 28, 20, EntityRadar::Kind::Object);
	const std::vector<EntityRadar::Entity> present{hidden};
	for (int i = 0; i < 5; ++i)
		registry.Observe(1, Discoverable(world, present), grid, {}, &present);
	Check(registry.Entities().size() == 1 && registry.Entities().front().position.x == 24,
		"known landmark remains at its last discovered position when currently hidden");
}

void ExplorationSurveyUsesOrientation() {
	Radar::World world = OpenWorld();
	WallWithGap(world, 18, 4);
	const Navigation::Grid grid = GridFor(world);
	const auto assisted = Survey(world, grid, {Make(1, 20, 16)}, true);
	const auto exploration = Survey(world, grid, {Make(1, 20, 16)}, false);
	Check(assisted.front().steps > 10, "full assistance keeps its walking route distance");
	Check(!exploration.front().reachable && exploration.front().steps < 0 && exploration.front().straightLine < 5,
		"exploration reports only direct distance and direction");
}

void ReturningToLevelRetainsFixedLandmarks() {
	Radar::World world = OpenWorld();
	const Navigation::Grid grid = GridFor(world);
	Memory memory;
	auto waypoint = Make(1, 24, 20, EntityRadar::Kind::Object);
	waypoint.category = EntityRadar::Category::Waypoint;
	const auto enemy = Make(2, 25, 20, EntityRadar::Kind::Enemy);
	const auto loot = Make(3, 26, 20, EntityRadar::Kind::Item);
	memory.Observe(1, {waypoint, enemy, loot}, grid, true);
	memory.Observe(2, {}, grid, true);
	Check(memory.Entities().empty(), "other level does not show the previous level's entries");
	memory.Select(1, true);
	Check(memory.Entities().size() == 1 && memory.Entities().front().id == waypoint.id,
		"returning in the same session retains a known waypoint but no enemy or loot");
	memory.Clear();
	memory.Select(1, true);
	Check(memory.Entities().empty(), "a new game or mode reset forgets session landmarks");
}

void FullAssistanceKeepsLevelChangeReset() {
	Radar::World world = OpenWorld();
	const Navigation::Grid grid = GridFor(world);
	Memory memory;
	memory.Observe(1, {Make(1, 24, 20, EntityRadar::Kind::Object)}, grid, false);
	memory.Select(2, false);
	memory.Select(1, false);
	Check(memory.Entities().empty(), "full assistance retains its level-change reset");
}

}

int main() {
	StepsGoAroundWalls();
	UnreachableIsReportedAsSuch();
	NearestByStepsNotByStraightLine();
	ReachableSortsAheadOfUnreachable();
	OutOfViewObjectsAreKept();
	AbsenceOutsideTheMapIsNotForgetting();
	ChangingLevelClears();
	ReobservingRefreshesPosition();
	KindFiltering();
	CategoryCycling();
	ExplorationDiscoversOnlyNearbyVisibleTargets();
	KnownLandmarkSurvivesLossOfSight();
	ExplorationSurveyUsesOrientation();
	ReturningToLevelRetainsFixedLandmarks();
	FullAssistanceKeepsLevelChangeReset();
	Passed();
	return 0;
}
