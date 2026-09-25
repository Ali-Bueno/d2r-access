#include "beacon_navigation.h"
#include "navigation_policy.h"

#include <cmath>
#include <utility>

namespace D2RAccess::BeaconNavigation {
namespace {
bool ReachableNow(const Navigation::Grid& grid, Radar::Position position, int margin) {
	const int x = static_cast<int>(std::lround(position.x)), y = static_cast<int>(std::lround(position.y));
	// Targets occupy their own cells; any cell within the margin reaches them.
	for (int dy = -margin; dy <= margin; ++dy)
		for (int dx = -margin; dx <= margin; ++dx) {
			const auto* cell = grid.At(x + dx, y + dy);
			if (cell && cell->steps >= 0) return true;
		}
	return false;
}
}

Guidance Prepare(Radar::World world, Radar::Position target, const RoomGraph::Graph* graph,
	const std::vector<NativeRooms::Room>* visited, int margin) {
	Guidance result;
	result.target = target;
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
	// The route's own layer: monsters in a doorway must not break the room route mid-fight.
	const auto grid = Navigation::Build(result.world, Navigation::MaxRadius, Radar::Layer::Terrain);
	if (ReachableNow(grid, target, margin)) return result;
	const auto goal = RoomGraph::ChooseSubGoal(*graph, grid, from, to);
	// No waypoint: the direct search decides, and exploration still requires it to arrive.
	if (!goal.known) return result;
	result.hasWaypoint = true;
	result.waypoint = goal.position;
	result.waypointRoom = graph->rooms[goal.room];
	result.inNextRoom = goal.inNextRoom;
	return result;
}
}
