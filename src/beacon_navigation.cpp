#include "beacon_navigation.h"
#include "navigation_policy.h"

#include <cmath>
#include <utility>

namespace D2RAccess::BeaconNavigation {
namespace {
bool ReachableNow(const Navigation::Grid& grid, Radar::Position position) {
	const int x = static_cast<int>(std::lround(position.x)), y = static_cast<int>(std::lround(position.y));
	// Targets can occupy their own cell; use the same immediate neighbours as the catalogue.
	for (int dy = -1; dy <= 1; ++dy)
		for (int dx = -1; dx <= 1; ++dx) {
			const auto* cell = grid.At(x + dx, y + dy);
			if (cell && cell->steps >= 0) return true;
		}
	return false;
}
}

Guidance Prepare(Radar::World world, Radar::Position target, const RoomGraph::Graph* graph,
	const std::vector<NativeRooms::Room>* visited) {
	Guidance result;
	RoomGraph::Graph restricted;
	if (visited) {
		if (!NavigationPolicy::Contains(*visited, world.player) || !NavigationPolicy::Contains(*visited, target)) return result;
		world = NavigationPolicy::Restrict(world, *visited);
		restricted = RoomGraph::Build(*visited);
		graph = &restricted;
	}
	result.world = std::move(world);
	result.available = true;
	if (!graph) return result;
	result.roomCount = graph->rooms.size();
	const size_t from = NativeRooms::Containing(graph->rooms, result.world.player);
	const size_t to = NativeRooms::Containing(graph->rooms, target);
	if (from == NativeRooms::None || to == NativeRooms::None) {
		result.available = !visited;
		return result;
	}
	if (from == to) return result;
	const auto grid = Navigation::Build(result.world, Navigation::MaxRadius);
	if (ReachableNow(grid, target)) return result;
	const auto route = RoomGraph::Route(*graph, from, to);
	const auto goal = RoomGraph::ChooseSubGoal(*graph, grid, route);
	if (!goal.known) {
		result.available = !visited;
		return result;
	}
	result.hasWaypoint = true;
	result.waypoint = goal.position;
	result.waypointRoom = graph->rooms[goal.room];
	result.inNextRoom = goal.inNextRoom;
	return result;
}
}
