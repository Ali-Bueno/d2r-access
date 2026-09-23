#include "zone_transitions.h"
#include "test_support.h"

#include <limits>
#include <vector>

using namespace D2RAccess;

namespace {
constexpr uint16_t PlayerPath = 0x1C09; // observed native walking mask
constexpr uint16_t Wall = 1;

Radar::RoomGrid Room(int x, int y, int width, int height) {
	Radar::RoomGrid room{x, y, width, height, std::vector<uint16_t>(static_cast<size_t>(width) * height, 0)};
	for (int i = 0; i < width; ++i) {
		room.cells[static_cast<size_t>(i)] = Wall;
		room.cells[static_cast<size_t>(height - 1) * width + i] = Wall;
	}
	for (int i = 0; i < height; ++i) {
		room.cells[static_cast<size_t>(i) * width] = Wall;
		room.cells[static_cast<size_t>(i) * width + width - 1] = Wall;
	}
	return room;
}
void Carve(Radar::RoomGrid& room, int x, int y) {
	room.cells[static_cast<size_t>(y - room.y) * room.width + x - room.x] = 0;
}
Radar::World World(Radar::RoomGrid own, Radar::RoomGrid adjacent, Radar::Position player, int bodyPattern = 0) {
	Radar::World world;
	world.session = world.level = 1;
	world.player = player;
	world.bodyPattern = bodyPattern;
	world.walkingMask = PlayerPath;
	world.rooms = {std::move(own), std::move(adjacent)};
	return world;
}
NativeLevelExits::Layout Layout(int heightTiles) {
	NativeLevelExits::Layout layout;
	layout.level = 1;
	layout.rooms = {{{0, 0, 2, heightTiles}, true}};
	layout.neighbours = {{{2, 0, 2, heightTiles}, 2}};
	return layout;
}
}

int main() {
	auto own = Room(0, 0, 10, 10), adjacent = Room(10, 0, 10, 10);
	for (int y = 4; y <= 6; ++y) { Carve(own, 9, y); Carve(adjacent, 10, y); }
	auto world = World(own, adjacent, {6, 5}, 2);
	auto layout = Layout(2);
	auto found = ZoneTransitions::Find(world, layout);
	Check(found.size() == 1 && found[0].level == 2 && found[0].position.x == 9 && found[0].position.y == 5,
		"an actual three-cell opening into Blood Moor creates one zone transition at the opening");
	world.player = {8, 4};
	found = ZoneTransitions::Find(world, layout);
	Check(found.size() == 1 && found[0].position.y == 5,
		"a square body uses the safe middle of a three-cell gate when nearer its blocked edge");
	world.player = {6, 5};

	layout.neighbours.clear();
	Check(ZoneTransitions::Find(world, layout).empty(), "an opening into an ordinary room is not a zone transition");
	layout = Layout(2);
	Carve(adjacent, 10, 5); // retain only one open cell on the far side
	adjacent.cells[static_cast<size_t>(4) * adjacent.width] = Wall;
	adjacent.cells[static_cast<size_t>(6) * adjacent.width] = Wall;
	world = World(own, adjacent, {6, 5}, 2);
	Check(ZoneTransitions::Find(world, layout).empty(), "a gap too narrow for the player's body is not a crossing");
	world = World(own, Room(10, 0, 10, 10), {6, 5}, 0);
	Check(ZoneTransitions::Find(world, layout).empty(), "a gap facing collision in the next level is silent");

	auto largeOwn = Room(0, 0, 10, 100), largeNext = Room(10, 0, 10, 100);
	for (int y = 20; y <= 80; ++y) { Carve(largeOwn, 9, y); Carve(largeNext, 10, y); }
	world = World(largeOwn, largeNext, {8, 20});
	layout = Layout(20);
	found = ZoneTransitions::Find(world, layout);
	Check(found.size() == 1 && found[0].position.x == 9 && found[0].position.y == 20,
		"a long open border sounds at the nearest passable point rather than its distant midpoint");
	world.player = {50, 20};
	Check(ZoneTransitions::Find(world, layout).empty(), "a crossing outside the audible radius does not create a source");

	own = Room(0, 0, 10, 10); adjacent = Room(10, 0, 10, 10);
	for (int y : {2, 7}) { Carve(own, 9, y); Carve(adjacent, 10, y); }
	world = World(own, adjacent, {7, 7});
	layout = Layout(2);
	layout.neighbours.push_back(layout.neighbours.front());
	found = ZoneTransitions::Find(world, layout);
	Check(found.size() == 1 && found[0].position.y == 7,
		"repeated neighbour records and multiple gaps keep one nearest cue per destination");
	layout.level = 9;
	Check(ZoneTransitions::Find(world, layout).empty(), "a stale layout from another level is ignored");
	layout.level = 1;
	world.player.x = std::numeric_limits<double>::quiet_NaN();
	Check(ZoneTransitions::Find(world, layout).empty(), "invalid player coordinates do not become collision-grid indices");
	Passed();
}
