#include "navigation_policy.h"
#include "beacon.h"
#include "beacon_navigation.h"
#include "test_support.h"
#include <algorithm>
#include <cmath>
#include <vector>

using namespace D2RAccess;
namespace {
// A monster's cell: MONSTER plus the NO_PATH it stamps (radar_grid.cpp), which
// the native walking mask blocks.
constexpr uint16_t NoPath = 0x1000, MonsterCell = 0x100 | NoPath;

NativeRooms::Room Room(int x, int y, int width = 8, int height = 8) {
	return {x, y, width, height, NativeRooms::Identity(x, y)};
}

Radar::World OpenWorld() {
	Radar::World world;
	world.player = {10, 20};
	world.walkingMask = Radar::Collision::Wall;
	world.rooms.push_back({0, 0, 80, 40, std::vector<uint16_t>(80 * 40, 0)});
	world.rooms.push_back({80, 0, 40, 40, std::vector<uint16_t>(40 * 40, 0)});
	return world;
}
}

int main() {
	const auto first = Room(0, 0), middle = Room(8, 0), last = Room(16, 0);
	const std::vector<NativeRooms::Room> visited{first, last};
	Check(NavigationPolicy::Contains(visited, {0, 0}), "a visited room includes its first subtile");
	Check(NavigationPolicy::Contains(visited, {39, 39}), "a visited room includes its final subtile");
	Check(!NavigationPolicy::Contains(visited, {40, 20}), "the next room begins outside the first room");
	Check(!NavigationPolicy::Contains(visited, {NAN, 20}), "an invalid position cannot qualify");

	auto world = OpenWorld();
	world.rooms[0].cells[20 * 80 + 12] = Radar::Collision::Wall;
	world.rooms[0].cells[20 * 80 + 13] = Radar::Collision::Blank;
	auto restricted = NavigationPolicy::Restrict(world, visited);
	Check(restricted.Cell(10, 20) == Radar::State::Open, "known open collision stays open");
	Check(restricted.Cell(12, 20) == Radar::State::Blocked, "known wall collision stays blocked");
	Check(restricted.Cell(13, 20) == Radar::State::Unknown, "a native blank remains unknown");
	Check(restricted.Cell(40, 20) == Radar::State::Unknown, "an unvisited room is unknown at the border");
	Check(restricted.Cell(79, 20) == Radar::State::Unknown, "mask covers the whole unvisited middle room");
	Check(restricted.Cell(80, 20) == Radar::State::Open, "known room on another collision grid stays open");
	Check(world.Cell(40, 20) == Radar::State::Open, "the original collision snapshot is unchanged");
	Check(restricted.Body(39, 20, Radar::Layer::Terrain) == Radar::State::Open,
		"the point footprint can stand at the last known cell");
	restricted.bodyPattern = 1;
	Check(restricted.Body(39, 20, Radar::Layer::Terrain) == Radar::State::Unknown,
		"a larger body cannot fit across an unknown border");

	const Beacon::Position destination{90, 20};
	const auto unrestrictedRoute = Beacon::FindRoute(world, destination, {});
	Check(unrestrictedRoute.reachable, "the unmasked fixture has a complete open route");
	const auto maskedRoute = Beacon::FindRoute(NavigationPolicy::Restrict(world, visited), destination, {});
	Check(!maskedRoute.reachable && !maskedRoute.straight,
		"visited rooms separated by an unknown room never produce a complete route");
	Check(!maskedRoute.path.empty() && maskedRoute.path.back().x < 40,
		"a partial route stops on the visited side of the unknown boundary");
	const auto empty = NavigationPolicy::Restrict(world, {});
	Check(empty.Cell(10, 20) == Radar::State::Unknown,
		"unavailable history makes even the loaded player room unknown");
	Check(!Beacon::FindRoute(empty, destination, {}).reachable,
		"an empty history cannot produce a complete route");

	const auto united = NavigationPolicy::Restrict(world, {first, middle, last});
	Check(Beacon::FindRoute(united, destination, {}).reachable,
		"adjacent visited rooms permit a complete collision route");

	const auto open = OpenWorld();
	const Radar::Position far{100, 20};
	const auto graph = RoomGraph::Build({first, middle, last});
	Check(BeaconNavigation::Prepare(open, far, &graph).available,
		"full assistance plans through rooms the player never entered");
	const auto gap = BeaconNavigation::Prepare(open, far, &graph, &visited);
	Check(!gap.hasWaypoint && !Beacon::Reaches(gap.world, Beacon::FindRoute(gap.world, far, {}), far, {}),
		"exploration finds no way across an unvisited room");
	const std::vector<NativeRooms::Room> start{first};
	Check(!BeaconNavigation::Prepare(open, far, &graph, &start).available,
		"exploration refuses a target in a room never entered");
	const std::vector<NativeRooms::Room> all{first, middle, last};
	const auto known = BeaconNavigation::Prepare(open, far, &graph, &all);
	Check(known.available && known.world.Cell(60, 20) == Radar::State::Open,
		"exploration plans through visited rooms on their known collision");
	const auto near = BeaconNavigation::Prepare(open, {30, 20}, &graph, &start);
	Check(near.available && !near.hasWaypoint && near.world.Cell(60, 20) == Radar::State::Unknown,
		"a target in the player's own room needs no waypoint and sees no unvisited floor");

	// Five rooms in a row, the target's beyond the collision window, and a pack
	// of monsters holding the only doorway out of the player's room.
	Radar::World fight;
	fight.player = {10, 20};
	fight.walkingMask = Radar::Collision::Wall | NoPath;
	fight.rooms.push_back({0, 0, 160, 40, std::vector<uint16_t>(160 * 40, 0)});
	auto& floor = fight.rooms.front().cells;
	for (int y = 0; y < 40; ++y) floor[static_cast<size_t>(y) * 160 + 40] = y >= 19 && y <= 21 ? MonsterCell : Radar::Collision::Wall;
	std::vector<NativeRooms::Room> row;
	for (int i = 0; i < 5; ++i) row.push_back(Room(i * 8, 0));
	const auto rowGraph = RoomGraph::Build(row);
	const Radar::Position exit{190, 20};
	const auto fighting = BeaconNavigation::Prepare(fight, exit, &rowGraph, &row);
	Check(fighting.available && fighting.hasWaypoint && fighting.waypointRoom.tileX == 24,
		"monsters in the only doorway do not break the room route");
	for (int y = 19; y <= 21; ++y) floor[static_cast<size_t>(y) * 160 + 40] = Radar::Collision::Wall;
	Check(!BeaconNavigation::Prepare(fight, exit, &rowGraph, &row).hasWaypoint, "a wall in the same doorway does");
	Check(fighting.target.x == exit.x && fighting.target.y == exit.y, "the guidance keeps the target it was given");
	Passed();
}
