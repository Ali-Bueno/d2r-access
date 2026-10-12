#pragma once

#include "level_route.h"
#include "room_graph.h"

#include <cstdint>
#include <vector>

namespace D2RAccess::BeaconNavigation {
struct Guidance {
	Radar::World world;
	Radar::Position target{}; // what the route leads to, as given to Prepare
	bool available = false;
	bool hasWaypoint = false;
	Radar::Position waypoint{};
	NativeRooms::Room waypointRoom{};
	bool inNextRoom = false;
	size_t roomCount = 0;
	// The remembered map was asked for the way: the waypoint, when there is
	// one, is as far along its way as the loaded floor goes, and when there is
	// none the map knows no way and nothing is guessed in its place.
	bool remembered = false;
	bool guess = false;  // part of that way is ground not seen yet
	size_t wayCells = 0; // the cells of that way
};

// The way over the level's remembered map (level_route.h) as guidance may take
// it: past shut doors, which the player opens, and over explored rooms alone in
// exploration mode. A search of the whole level, so the map is copied to plan on
// at most every RefreshMs, and a moving place is planned to where it was.
class RememberedWay {
public:
	static constexpr uint64_t RefreshMs = 1000;
	static constexpr double PlaceDrift = 10; // cells a place may move before it is planned to afresh
	// `explored` keeps the way to those rooms; null for the whole map.
	LevelRoute::Way From(const LevelRoute::Memory& memory, const std::vector<NativeRooms::Room>* explored,
		Radar::Position from, Radar::Position place, int margin, int bodyPattern, uint64_t now);
	void Clear() { *this = {}; }
private:
	LevelRoute::Memory view_;
	LevelRoute::Plan plan_;
	std::vector<uint64_t> rooms_; // the explored rooms' ids the view keeps, sorted
	bool restricted_ = false;
	uint64_t copies_ = 0, copiedRevision_ = 0, copiedMs_ = 0, placedMs_ = 0;
	Radar::Position place_{};
};

// The level's remembered map and the guidance's way over it.
struct Recall {
	const LevelRoute::Memory* memory = nullptr;
	RememberedWay* way = nullptr;
	uint64_t now = 0;
};

// A null visited list retains full assistance; an empty list grants no exploration route.
// `margin` is how many cells around the target count as reaching it.
// `built` is the terrain grid of this same `world` at the full radius when the
// caller has one already; with full assistance it is used instead of building
// the grid a second time.
// With `recall`, a target the loaded floor does not reach is led to along the
// remembered map's way, never by the room graph's guess: docs/beacon.md#walkable-guidance.
Guidance Prepare(Radar::World world, Radar::Position target, const RoomGraph::Graph* graph,
	const std::vector<NativeRooms::Room>* visited = nullptr, int margin = 1, const Navigation::Grid* built = nullptr,
	const Recall& recall = {});
}
