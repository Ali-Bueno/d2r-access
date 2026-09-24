#pragma once

#include "room_graph.h"

namespace D2RAccess::BeaconNavigation {
struct Guidance {
	Radar::World world;
	bool available = false;
	bool hasWaypoint = false;
	Radar::Position waypoint{};
	NativeRooms::Room waypointRoom{};
	bool inNextRoom = false;
	size_t roomCount = 0;
};

// A null visited list retains full assistance; an empty list grants no exploration route.
Guidance Prepare(Radar::World world, Radar::Position target, const RoomGraph::Graph* graph,
	const std::vector<NativeRooms::Room>* visited = nullptr);
}
