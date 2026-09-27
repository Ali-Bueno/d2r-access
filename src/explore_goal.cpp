#include "explore_goal.h"

#include <algorithm>
#include <cmath>

namespace D2RAccess::ExploreGoal {
namespace {
// Squared distance from a point to a room, zero inside it.
double DistanceSquared(const NativeRooms::Bounds& bounds, Position point) {
	const double dx = point.x < bounds.minX ? bounds.minX - point.x : (point.x >= bounds.maxX ? point.x - (bounds.maxX - 1) : 0);
	const double dy = point.y < bounds.minY ? bounds.minY - point.y : (point.y >= bounds.maxY ? point.y - (bounds.maxY - 1) : 0);
	return dx * dx + dy * dy;
}
}

Choice Nearest(const RoomGraph::Graph& graph, const Navigation::Grid& grid,
	const std::vector<NativeRooms::Room>& explored, Position player, const std::vector<uint64_t>& refused) {
	Choice choice;
	const size_t from = NativeRooms::Containing(graph.rooms, player);
	if (from == NativeRooms::None) return choice;
	// Hops are the same in both directions, so one search from the player's
	// room measures every room of the level.
	const auto hops = RoomGraph::Distances(graph, from, &grid);
	double nearest = 0;
	for (size_t room = 0; room < graph.rooms.size(); ++room) {
		if (room == from || hops[room] == RoomGraph::NoRoute) continue;
		const auto id = graph.rooms[room].id;
		if (std::any_of(explored.begin(), explored.end(), [id](const auto& known) { return known.id == id; })
			|| std::find(refused.begin(), refused.end(), id) != refused.end()) continue;
		const auto bounds = NativeRooms::Subtiles(graph.rooms[room]);
		const double distance = DistanceSquared(bounds, player);
		if (choice.found && (hops[room] > choice.hops || (hops[room] == choice.hops && distance >= nearest))) continue;
		choice.found = true;
		choice.room = room;
		choice.hops = hops[room];
		choice.position = {(bounds.minX + bounds.maxX - 1) / 2.0, (bounds.minY + bounds.maxY - 1) / 2.0};
		nearest = distance;
	}
	return choice;
}
}
