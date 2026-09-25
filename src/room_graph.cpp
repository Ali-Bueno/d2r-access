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

// The part of a room that the grid currently covers, as a half-open cell
// rectangle. Empty when the room is outside the collision window.
struct Window { int minX = 0, minY = 0, maxX = 0, maxY = 0; bool Empty() const { return minX >= maxX || minY >= maxY; } };
Window Overlap(const Navigation::Grid& grid, const NativeRooms::Bounds& bounds) {
	return {(std::max)(bounds.minX, grid.x), (std::max)(bounds.minY, grid.y),
		(std::min)(bounds.maxX, grid.x + grid.width), (std::min)(bounds.maxY, grid.y + grid.height)};
}

bool Known(const Navigation::Cell* cell) { return cell != nullptr && cell->body != Radar::State::Unknown; }
bool Open(const Navigation::Cell* cell) { return cell != nullptr && cell->body == Radar::State::Open; }

// Squared distance from a cell to a rectangle, zero inside it. Integer, so
// ordering is exact and there is no tolerance to tune.
long long DistanceSquared(const NativeRooms::Bounds& bounds, int x, int y) {
	const long long dx = x < bounds.minX ? bounds.minX - x : (x >= bounds.maxX ? x - (bounds.maxX - 1) : 0);
	const long long dy = y < bounds.minY ? bounds.minY - y : (y >= bounds.maxY ? y - (bounds.maxY - 1) : 0);
	return dx * dx + dy * dy;
}

// Any collision for this room inside the window: the game has built it.
bool Loaded(const Navigation::Grid& grid, const NativeRooms::Room& room) {
	const Window window = Overlap(grid, NativeRooms::Subtiles(room));
	for (int y = window.minY; y < window.maxY; ++y)
		for (int x = window.minX; x < window.maxX; ++x)
			if (Known(grid.At(x, y))) return true;
	return false;
}

}

bool Near(const NativeRooms::Room& a, const NativeRooms::Room& b) {
	return Gap(a.tileX, a.tileWidth, b.tileX, b.tileWidth) < NearGapTiles
		&& Gap(a.tileY, a.tileHeight, b.tileY, b.tileHeight) < NearGapTiles;
}

bool Touching(const NativeRooms::Room& a, const NativeRooms::Room& b) {
	const int gapX = Gap(a.tileX, a.tileWidth, b.tileX, b.tileWidth);
	const int gapY = Gap(a.tileY, a.tileHeight, b.tileY, b.tileHeight);
	return (gapX <= 0 && gapY < 0) || (gapX < 0 && gapY <= 0);
}

Passage Border(const Navigation::Grid& grid, const NativeRooms::Room& a, const NativeRooms::Room& b) {
	const auto first = NativeRooms::Subtiles(a), second = NativeRooms::Subtiles(b);
	const bool across = first.maxX == second.minX || second.maxX == first.minX;
	const bool along = first.maxY == second.minY || second.maxY == first.minY;
	if (across == along) return Passage::Unknown; // overlapping, or meeting at a corner only
	// Walk the edge as (position along it, depth into the room on each side).
	const int start = across ? (std::max)(first.minY, second.minY) : (std::max)(first.minX, second.minX);
	const int end = across ? (std::min)(first.maxY, second.maxY) : (std::min)(first.maxX, second.maxX);
	const int sideA = across ? (first.maxX == second.minX ? first.maxX - 1 : first.minX)
		: (first.maxY == second.minY ? first.maxY - 1 : first.minY);
	const int sideB = across ? (first.maxX == second.minX ? second.minX : second.maxX - 1)
		: (first.maxY == second.minY ? second.minY : second.maxY - 1);
	const auto at = [&](int along, int depth) { return across ? grid.At(depth, along) : grid.At(along, depth); };
	bool knownA = true, knownB = true, openA = false, openB = false;
	for (int i = start; i < end; ++i) {
		const auto* cellA = at(i, sideA);
		const auto* cellB = at(i, sideB);
		knownA = knownA && Known(cellA);
		knownB = knownB && Known(cellB);
		openA = openA || Open(cellA);
		openB = openB || Open(cellB);
		if (!Open(cellA)) continue;
		for (int d = -1; d <= 1; ++d)
			if (i + d >= start && i + d < end && Open(at(i + d, sideB))) return Passage::Open;
	}
	return (knownA && !openA) || (knownB && !openB) || (knownA && knownB) ? Passage::Closed : Passage::Unknown;
}

Graph Build(const std::vector<NativeRooms::Room>& rooms) {
	Graph graph;
	graph.rooms = rooms;
	graph.links.resize(rooms.size());
	for (size_t i = 0; i < rooms.size(); ++i)
		for (size_t j = i + 1; j < rooms.size(); ++j)
			if (Touching(rooms[i], rooms[j])) {
				graph.links[i].push_back(j);
				graph.links[j].push_back(i);
			}
	return graph;
}

std::vector<int> Distances(const Graph& graph, size_t to, const Navigation::Grid* grid) {
	std::vector<int> hops(graph.rooms.size(), NoRoute);
	if (to >= graph.rooms.size()) return hops;
	std::deque<size_t> queue{to};
	hops[to] = 0;
	while (!queue.empty()) {
		const size_t at = queue.front();
		queue.pop_front();
		for (const size_t next : graph.links[at]) {
			if (hops[next] != NoRoute) continue;
			if (grid && Border(*grid, graph.rooms[at], graph.rooms[next]) == Passage::Closed) continue;
			hops[next] = hops[at] + 1;
			queue.push_back(next);
		}
	}
	return hops;
}

SubGoal ChooseSubGoal(const Graph& graph, const Navigation::Grid& grid, size_t from, size_t to) {
	SubGoal goal;
	const auto hops = Distances(graph, to, &grid);
	if (from >= hops.size() || hops[from] == NoRoute) return goal;
	// 1. The reachable cell in the room fewest hops from the target, strictly
	//    nearer than this one; within that room, the fewest steps: its doorway.
	for (size_t room = 0; room < graph.rooms.size(); ++room) {
		if (hops[room] == NoRoute || hops[room] >= hops[from]) continue;
		if (goal.known && hops[room] > hops[goal.room]) continue;
		const Window window = Overlap(grid, NativeRooms::Subtiles(graph.rooms[room]));
		for (int y = window.minY; y < window.maxY; ++y)
			for (int x = window.minX; x < window.maxX; ++x) {
				const auto* cell = grid.At(x, y);
				if (cell == nullptr || cell->steps < 0) continue;
				if (goal.known && hops[room] == hops[goal.room] && cell->steps >= goal.steps) continue;
				goal = {true, room, {static_cast<double>(x), static_cast<double>(y)}, cell->steps, true};
			}
	}
	if (goal.known) return goal;
	// 2. A nearer neighbour with no collision yet: the reachable cell of this
	//    room closest to it, since walking there is what loads it. Never toward
	//    a loaded room: its walls are known, and none of it was reachable.
	const Window current = Overlap(grid, NativeRooms::Subtiles(graph.rooms[from]));
	long long closest = (std::numeric_limits<long long>::max)();
	for (const size_t next : graph.links[from]) {
		if (hops[next] == NoRoute || hops[next] >= hops[from] || Loaded(grid, graph.rooms[next])) continue;
		const auto bounds = NativeRooms::Subtiles(graph.rooms[next]);
		for (int y = current.minY; y < current.maxY; ++y)
			for (int x = current.minX; x < current.maxX; ++x) {
				const auto* cell = grid.At(x, y);
				if (cell == nullptr || cell->steps < 0) continue;
				const long long distance = DistanceSquared(bounds, x, y);
				if (distance > closest || (distance == closest && cell->steps >= goal.steps)) continue;
				closest = distance;
				goal = {true, from, {static_cast<double>(x), static_cast<double>(y)}, cell->steps, false};
			}
	}
	return goal;
}

}
