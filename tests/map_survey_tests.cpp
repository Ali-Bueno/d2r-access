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
	Passed();
	return 0;
}
