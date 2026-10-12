#include "beacon_navigation.h"
#include "navigation_policy.h"

#include <algorithm>
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

LevelRoute::Way RememberedWay::From(const LevelRoute::Memory& memory, const std::vector<NativeRooms::Room>* explored,
	Radar::Position from, Radar::Position place, int margin, int bodyPattern, uint64_t now) {
	std::vector<uint64_t> rooms;
	if (explored) {
		for (const auto& room : *explored) rooms.push_back(room.id);
		std::sort(rooms.begin(), rooms.end());
	}
	// Another map or other rooms are copied at once; the same map learning more, at most every RefreshMs.
	const bool other = !copies_ || view_.Session() != memory.Session() || view_.Level() != memory.Level()
		|| restricted_ != (explored != nullptr) || rooms != rooms_;
	const bool due = now < copiedMs_ || now - copiedMs_ >= RefreshMs;
	if (other || (due && memory.Revision() != copiedRevision_)) {
		view_ = memory.Copy(explored, ++copies_);
		copiedRevision_ = memory.Revision();
		copiedMs_ = now;
		rooms_ = std::move(rooms);
		restricted_ = explored != nullptr;
	}
	if (other || now < placedMs_ || now - placedMs_ >= RefreshMs || std::hypot(place.x - place_.x, place.y - place_.y) > PlaceDrift) {
		place_ = place;
		placedMs_ = now;
	}
	return plan_.From(view_, from, place_, margin, bodyPattern, true);
}

Guidance Prepare(Radar::World world, Radar::Position target, const RoomGraph::Graph* graph,
	const std::vector<NativeRooms::Room>* visited, int margin, const Navigation::Grid* built, const Recall& recall) {
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
	const bool remembered = recall.memory && recall.way;
	if (from == to && !remembered) return result;
	// The route's own layer: monsters in a doorway must not break the room route mid-fight.
	// A restricted world is not the one the caller's grid was built from.
	Navigation::Grid own;
	if (!built || visited) own = Navigation::Build(result.world, Navigation::MaxRadius, Radar::Layer::Terrain, false);
	const auto& grid = built && !visited ? *built : own;
	if (ReachableNow(grid, target, margin)) return result;
	if (remembered) {
		// The map's way turns where the floor turns; the room graph only knows which rooms touch.
		const auto way = recall.way->From(*recall.memory, visited, result.world.player, target, margin,
			result.world.bodyPattern, recall.now);
		result.remembered = true;
		result.guess = way.guess;
		result.wayCells = way.path.size();
		result.hasWaypoint = LevelRoute::Ahead(way.path, grid, result.waypoint);
		return result;
	}
	if (from == to) return result;
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
