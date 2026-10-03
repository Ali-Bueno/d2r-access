#pragma once

#include "room_graph.h"

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
};

// A null visited list retains full assistance; an empty list grants no exploration route.
// `margin` is how many cells around the target count as reaching it.
// `built` is the terrain grid of this same `world` at the full radius when the
// caller has one already; with full assistance it is used instead of building
// the grid a second time.
Guidance Prepare(Radar::World world, Radar::Position target, const RoomGraph::Graph* graph,
	const std::vector<NativeRooms::Room>* visited = nullptr, int margin = 1, const Navigation::Grid* built = nullptr);
}
