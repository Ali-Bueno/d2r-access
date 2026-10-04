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
	int bestSteps = -1;
	for (size_t room = 0; room < graph.rooms.size(); ++room) {
		if (room == from || hops[room] == RoomGraph::NoRoute) continue;
		const auto id = graph.rooms[room].id;
		if (std::any_of(explored.begin(), explored.end(), [id](const auto& known) { return known.id == id; })
			|| std::find(refused.begin(), refused.end(), id) != refused.end()) continue;
		const auto bounds = NativeRooms::Subtiles(graph.rooms[room]);
		const double distance = DistanceSquared(bounds, player);
		Position entry{(bounds.minX + bounds.maxX - 1) / 2.0, (bounds.minY + bounds.maxY - 1) / 2.0};
		// Enter the room at reachable floor, not its geometric centre: town
		// rectangles include water, walls and narrow paths. When it is beyond
		// the loaded collision window, the existing room/map guidance leads.
		int steps = -1;
		for (int y = std::max(bounds.minY, grid.y); y < std::min(bounds.maxY, grid.y + grid.height); ++y)
			for (int x = std::max(bounds.minX, grid.x); x < std::min(bounds.maxX, grid.x + grid.width); ++x) {
				const auto* cell = grid.At(x, y);
				if (!cell || cell->body != Radar::State::Open || cell->steps < 0 || (steps >= 0 && cell->steps >= steps)) continue;
				steps = cell->steps;
				entry = {static_cast<double>(x), static_cast<double>(y)};
			}
		// A proved walking route comes before a guess through unloaded scenery.
		// Among proved routes choose the nearest entry by actual walking steps.
		if (choice.found) {
			if (bestSteps >= 0 && (steps < 0 || steps >= bestSteps)) continue;
			if (bestSteps < 0 && steps < 0
				&& (hops[room] > choice.hops || (hops[room] == choice.hops && distance >= nearest))) continue;
		}
		choice = {true, room, entry, hops[room]};
		bestSteps = steps;
		nearest = distance;
	}
	return choice;
}
}
