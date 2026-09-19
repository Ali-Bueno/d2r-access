#include "room_graph.h"

#include <algorithm>
#include <deque>
#include <limits>

namespace D2RAccess::RoomGraph {
namespace {

// Gap between the facing edges on one axis, negative where they overlap.
// Mirrors the branch the native builder uses rather than a symmetric
// expression, so the two cannot disagree at the boundary.
int Gap(int aStart, int aSize, int bStart, int bSize) {
	return aStart < bStart ? bStart - (aStart + aSize) : aStart - (bStart + bSize);
}

// Squared distance from a cell to a rectangle, zero inside it. Integer, so
// ordering is exact and there is no tolerance to tune.
long long DistanceSquared(const NativeRooms::Bounds& bounds, int x, int y) {
	const long long dx = x < bounds.minX ? bounds.minX - x : (x >= bounds.maxX ? x - (bounds.maxX - 1) : 0);
	const long long dy = y < bounds.minY ? bounds.minY - y : (y >= bounds.maxY ? y - (bounds.maxY - 1) : 0);
	return dx * dx + dy * dy;
}

// The part of a room that the grid currently covers, as a half-open cell
// rectangle. Empty when the room is outside the collision window.
struct Window { int minX = 0, minY = 0, maxX = 0, maxY = 0; bool Empty() const { return minX >= maxX || minY >= maxY; } };
Window Overlap(const Navigation::Grid& grid, const NativeRooms::Bounds& bounds) {
	return {(std::max)(bounds.minX, grid.x), (std::max)(bounds.minY, grid.y),
		(std::min)(bounds.maxX, grid.x + grid.width), (std::min)(bounds.maxY, grid.y + grid.height)};
}

}

bool Near(const NativeRooms::Room& a, const NativeRooms::Room& b) {
	return Gap(a.tileX, a.tileWidth, b.tileX, b.tileWidth) < NearGapTiles
		&& Gap(a.tileY, a.tileHeight, b.tileY, b.tileHeight) < NearGapTiles;
}

Graph Build(const std::vector<NativeRooms::Room>& rooms) {
	Graph graph;
	graph.rooms = rooms;
	graph.near.resize(rooms.size());
	for (size_t i = 0; i < rooms.size(); ++i)
		for (size_t j = i + 1; j < rooms.size(); ++j)
			if (Near(rooms[i], rooms[j])) {
				graph.near[i].push_back(j);
				graph.near[j].push_back(i);
			}
	return graph;
}

std::vector<size_t> Route(const Graph& graph, size_t from, size_t to) {
	const size_t count = graph.rooms.size();
	if (from >= count || to >= count) return {};
	if (from == to) return {from};
	std::vector<size_t> previous(count, NativeRooms::None);
	std::deque<size_t>  queue{from};
	previous[from] = from;
	while (!queue.empty()) {
		const size_t at = queue.front();
		queue.pop_front();
		for (const size_t next : graph.near[at]) {
			if (previous[next] != NativeRooms::None) continue;
			previous[next] = at;
			if (next == to) {
				std::vector<size_t> route;
				for (size_t step = to; step != from; step = previous[step]) route.push_back(step);
				route.push_back(from);
				std::reverse(route.begin(), route.end());
				return route;
			}
			queue.push_back(next);
		}
	}
	return {};
}

SubGoal ChooseSubGoal(const Graph& graph, const Navigation::Grid& grid, const std::vector<size_t>& route) {
	SubGoal goal;
	if (route.size() < 2) return goal;
	const size_t current = route[0], next = route[1];
	if (current >= graph.rooms.size() || next >= graph.rooms.size()) return goal;
	const auto nextBounds = NativeRooms::Subtiles(graph.rooms[next]);

	// 1. The cheapest reachable cell inside the next room: its doorway.
	const Window inNext = Overlap(grid, nextBounds);
	if (!inNext.Empty()) {
		int best = -1, bestX = 0, bestY = 0;
		for (int y = inNext.minY; y < inNext.maxY; ++y)
			for (int x = inNext.minX; x < inNext.maxX; ++x) {
				const Navigation::Cell* cell = grid.At(x, y);
				if (cell == nullptr || cell->steps < 0) continue;
				if (best < 0 || cell->steps < best) best = cell->steps, bestX = x, bestY = y;
			}
		if (best >= 0) {
			goal.known = true, goal.room = next, goal.steps = best, goal.inNextRoom = true;
			goal.position = {static_cast<double>(bestX), static_cast<double>(bestY)};
			return goal;
		}
	}

	// 2. No collision there yet: hug the boundary from this side instead.
	const Window inCurrent = Overlap(grid, NativeRooms::Subtiles(graph.rooms[current]));
	if (inCurrent.Empty()) return goal;
	long long closest = (std::numeric_limits<long long>::max)();
	int       best = -1, bestX = 0, bestY = 0;
	for (int y = inCurrent.minY; y < inCurrent.maxY; ++y)
		for (int x = inCurrent.minX; x < inCurrent.maxX; ++x) {
			const Navigation::Cell* cell = grid.At(x, y);
			if (cell == nullptr || cell->steps < 0) continue;
			const long long distance = DistanceSquared(nextBounds, x, y);
			if (distance > closest || (distance == closest && best >= 0 && cell->steps >= best)) continue;
			closest = distance, best = cell->steps, bestX = x, bestY = y;
		}
	if (best < 0) return goal;
	goal.known = true, goal.room = current, goal.steps = best, goal.inNextRoom = false;
	goal.position = {static_cast<double>(bestX), static_cast<double>(bestY)};
	return goal;
}

}
