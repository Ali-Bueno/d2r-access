#include "explored_map.h"
#include "unexplored_route.h"
#include "test_support.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace D2RAccess;
using namespace UnexploredRoute;
namespace {
constexpr int Side = 50;
// The walking mask and open/blocked convention of the navigation grid's own
// tests: a zero cell is floor, bit 0 is a wall.
Radar::World Field(bool walled = false) {
	Radar::World world;
	world.session = world.level = 1;
	world.player = {20, 20};
	world.walkingMask = 0x1C09;
	world.rooms = {{0, 0, Side, Side, std::vector<uint16_t>(Side * Side, walled ? uint16_t{1} : uint16_t{0})}};
	return world;
}
void Put(Radar::World& world, int x, int y, uint16_t flags) { world.rooms[0].cells[static_cast<size_t>(y) * Side + x] = flags; }

Explored::Map Walked(Radar::Position position, int radius) {
	Explored::Map map;
	map.Follow(1, 1);
	map.Mark(position, {radius});
	return map;
}

Route RouteFrom(const Radar::World& world, const Explored::Map& map, Settings settings = {}) {
	settings.radius = 12;
	return Find(Navigation::Build(world, settings.radius), map, settings);
}

// One push of the stick: the screen directions are the world diagonals.
Radar::Position After(Radar::Position from, Radar::Direction direction, int count) {
	const double dx = direction == Radar::Direction::Up || direction == Radar::Direction::Left ? -1 : 1;
	const double dy = direction == Radar::Direction::Up || direction == Radar::Direction::Right ? -1 : 1;
	return {from.x + dx * count, from.y + dy * count};
}
// Walks the player, painting the memory on the way as the service's poll does.
void Walk(Radar::World& world, Explored::Map& map, Radar::Direction direction, int count, int brush) {
	for (int step = 0; step < count; ++step) {
		world.player = After(world.player, direction, 1);
		map.Mark(world.player, {brush});
	}
}

void Memory() {
	Explored::Map map;
	map.Follow(1, 5);
	map.Mark({20, 20}, {3});
	Check(map.Seen(20, 20) && map.Seen(22, 20), "the disc around the player is explored");
	Check(!map.Seen(24, 20), "beyond the brush is not");
	Check(map.Seen(22, 22) && !map.Seen(23, 22), "the brush is a disc, not a square: its corner stays unpainted");
	Check(map.Chunks() > 0 && map.Level() == 5, "the memory knows the level it belongs to");
	map.Follow(1, 6);
	Check(!map.Seen(20, 20) && map.Chunks() == 0, "a new level starts an empty memory");
	map.Mark({20, 20}, {3});
	map.Follow(1, 6);
	Check(map.Seen(20, 20), "the same level keeps it");
	map.Follow(2, 6);
	Check(!map.Seen(20, 20), "so does a new game, even on the same level number");
}

void NearestUnexplored() {
	const auto world = Field();
	Explored::Map fresh;
	fresh.Follow(1, 1);
	const auto first = RouteFrom(world, fresh);
	Check(first.outcome == Outcome::Route && first.distance > 0, "with nothing walked yet there is somewhere to go");

	const auto route = RouteFrom(world, Walked({20, 20}, 5));
	Check(route.outcome == Outcome::Route, "an open field around the walked disc has unexplored space");
	Check(route.steps.size() == 1 && route.steps[0].count == route.distance, "a straight run is spoken as one instruction");
}

// The bug the player reported: "down 9", two steps down, "down 9" again. The
// target was the first cell the memory did not cover — the rim of the brush the
// player carries, which retreats exactly as fast as he walks.
void TheTargetStaysPut() {
	auto world = Field(/*walled=*/true);
	for (int k = 5; k < 40; ++k) Put(world, k, k, 0); // one screen-vertical corridor
	Explored::Map map = Walked({20, 20}, 5);
	Settings settings;
	settings.radius = 12;
	settings.probeRadius = 5;
	const auto first = Find(Navigation::Build(world, settings.radius), map, settings);
	Check(first.outcome == Outcome::Route && first.distance >= 6, "the target sits well inside unexplored ground, not on the brush's rim");
	Check(first.steps.size() == 1 && first.steps[0].direction == Radar::Direction::Up, "up the corridor");

	Walk(world, map, Radar::Direction::Up, 3, 5);
	const auto second = Find(Navigation::Build(world, settings.radius), map, settings, {first.target});
	Check(second.keptLocked && second.target.x == first.target.x && second.target.y == first.target.y,
		"the next press heads for the same place");
	Check(second.distance == first.distance - 3, "and the distance has come down by the three steps walked");
	Check(second.steps.size() == 1 && second.steps[0].count == first.steps[0].count - 3, "the instruction counts down with it");

	Walk(world, map, Radar::Direction::Up, second.distance, 5);
	const auto arrival = Find(Navigation::Build(world, settings.radius), map, settings, {first.target});
	Check(arrival.arrived, "walking onto it explores it, and the lock is given up");
	Check(!arrival.keptLocked, "so the press that notices is already looking for the next place");
}

void ALockedTargetOutOfReachIsDropped() {
	auto world = Field(/*walled=*/true);
	for (int k = 5; k < 40; ++k) Put(world, k, k, 0);
	Explored::Map map = Walked({20, 20}, 5);
	Settings settings;
	settings.radius = 12;
	settings.probeRadius = 5;
	const Radar::Position elsewhere{40, 4}; // off the corridor, in the wall
	const auto route = Find(Navigation::Build(world, settings.radius), map, settings, {elsewhere});
	Check(!route.keptLocked && route.outcome == Outcome::Route, "a target that cannot be reached is replaced, not repeated");
}

void FollowsTheFloor() {
	// Everything is wall except one screen-vertical corridor: the world
	// diagonal x == y, which the stick walks with screen up and down.
	auto world = Field(/*walled=*/true);
	for (int k = 5; k < 40; ++k) Put(world, k, k, 0);
	const auto route = RouteFrom(world, Walked({20, 20}, 5));
	Check(route.outcome == Outcome::Route, "the corridor leads somewhere unexplored");
	Check(route.steps.size() == 1, "the corridor is one run");
	Check(route.steps[0].direction == Radar::Direction::Up && route.steps[0].count == 4, "and it is walked screen-up");

	// Close the corridor behind the walked part: the only way out is the other end.
	for (int k = 5; k <= 17; ++k) Put(world, k, k, 1);
	const auto away = RouteFrom(world, Walked({20, 20}, 5));
	Check(away.outcome == Outcome::Route && away.steps.size() == 1, "a blocked end leaves a single way");
	Check(away.steps[0].direction == Radar::Direction::Down, "which is the other direction");
}

void TurnsAndTruncation() {
	// An L: walk screen-up, then screen-right.
	auto world = Field(/*walled=*/true);
	for (int k = 0; k <= 8; ++k) Put(world, 20 - k, 20 - k, 0);
	for (int k = 1; k <= 6; ++k) Put(world, 12 + k, 12 - k, 0);
	const auto route = RouteFrom(world, Walked({20, 20}, 5));
	Check(route.outcome == Outcome::Route, "the corner is reachable");
	Check(route.steps.size() >= 1 && route.steps[0].direction == Radar::Direction::Up, "the first instruction starts along the corridor");
	Settings shortened;
	shortened.radius = 12;
	shortened.maxRuns = 1;
	const auto clipped = Find(Navigation::Build(world, 12), Walked({20, 20}, 5), shortened);
	Check(clipped.steps.size() <= 1, "a long route is given one run at a time");
}

void NothingLeft() {
	auto world = Field(/*walled=*/true);
	for (int y = 19; y <= 21; ++y) for (int x = 19; x <= 21; ++x) Put(world, x, y, 0);
	const auto route = RouteFrom(world, Walked({20, 20}, 6));
	Check(route.outcome == Outcome::None, "a walked closed room has nothing unexplored left");
	Check(Describe(route, Strings::For(Strings::Language::enUS)) == "No unexplored space nearby", "and says so");
}

void BearingWithoutARoute() {
	// A corridor along the world X axis: no screen direction walks it, because
	// each screen step is a world diagonal.
	auto world = Field(/*walled=*/true);
	for (int x = 14; x <= 26; ++x) Put(world, x, 20, 0);
	const auto route = RouteFrom(world, Walked({20, 20}, 3));
	Check(route.outcome == Outcome::Bearing, "the place is reachable, but not along the four directions");
	Check(!route.steps.empty(), "a bearing is still given");
	const auto text = Describe(route, Strings::For(Strings::Language::enUS));
	Check(text.find("no route, direction only") != std::string::npos, "and it says it is only a bearing");
}

void Unavailable() {
	Radar::World empty;
	Explored::Map map;
	const auto route = Find(Navigation::Build(empty, 12), map, {});
	Check(route.outcome == Outcome::Unavailable, "no world means no route");
	Check(Describe(route, Strings::For(Strings::Language::enUS)) == "Route unavailable", "and the speech says so");
}

void Wording() {
	Route route;
	route.outcome = Outcome::Route;
	route.steps = {{Radar::Direction::Up, 6}, {Radar::Direction::Right, 4}};
	Check(Describe(route, Strings::For(Strings::Language::enUS)) == "Nearest unexplored space, up 6, right 4",
		"the route reads as walking instructions");
	Check(Describe(route, Strings::For(Strings::Language::plPL)) == "Najbliższa niezbadana przestrzeń, w górę 6, w prawo 4",
		"in the player's own language");
	Route here;
	here.outcome = Outcome::Route;
	Check(Describe(here, Strings::For(Strings::Language::enUS)).find("right here") != std::string::npos,
		"standing on it says so instead of an empty instruction");
}
}

int main() {
	Memory();
	NearestUnexplored();
	TheTargetStaysPut();
	ALockedTargetOutOfReachIsDropped();
	FollowsTheFloor();
	TurnsAndTruncation();
	NothingLeft();
	BearingWithoutARoute();
	Unavailable();
	Wording();
	Passed();
	return 0;
}
