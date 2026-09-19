#include "unexplored_route.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdio>
#include <vector>

namespace D2RAccess::UnexploredRoute {
namespace {
struct Move {
	Radar::Direction direction;
	int dx, dy;
	uint8_t exit; // Navigation::DirectionBits of this step
};
// The four screen directions of the left stick, which are the world diagonals.
constexpr std::array<Move, 4> ScreenMoves{{
	{Radar::Direction::Up, -1, -1, Navigation::MinusXMinusY},
	{Radar::Direction::Left, -1, 1, Navigation::MinusXPlusY},
	{Radar::Direction::Right, 1, -1, Navigation::PlusXMinusY},
	{Radar::Direction::Down, 1, 1, Navigation::PlusXPlusY}}};
// Plus the world axes, which the stick can only reach between two of its
// directions: good enough to prove a place is reachable, not to be spoken.
constexpr std::array<Move, 8> AllMoves{{ScreenMoves[0], ScreenMoves[1], ScreenMoves[2], ScreenMoves[3],
	{Radar::Direction::Up, -1, 0, Navigation::MinusX}, {Radar::Direction::Down, 1, 0, Navigation::PlusX},
	{Radar::Direction::Up, 0, -1, Navigation::MinusY}, {Radar::Direction::Down, 0, 1, Navigation::PlusY}}};
constexpr std::array<int, 8> NeighbourX{-1, 1, 0, 0, -1, 1, -1, 1};
constexpr std::array<int, 8> NeighbourY{0, 0, -1, 1, -1, -1, 1, 1};

// One breadth-first sweep of everything the player can walk to.
struct Flood {
	bool ok = false;
	size_t origin = 0;
	std::vector<int> parent;    // previous cell, -1 at the origin, -2 unvisited
	std::vector<int> incoming;  // move that reached the cell
	std::vector<size_t> order;  // visit order: nearest first, in steps walked
};

size_t MovementIndex(Radar::Direction direction) {
	// Strings::Table::radarMovementDirections runs clockwise from screen up.
	switch (direction) {
	case Radar::Direction::Up: return 0;
	case Radar::Direction::Right: return 2;
	case Radar::Direction::Down: return 4;
	default: return 6;
	}
}

size_t Origin(const Navigation::Grid& grid) {
	return static_cast<size_t>(grid.playerY - grid.y) * grid.width + static_cast<size_t>(grid.playerX - grid.x);
}

template <size_t Count>
Flood Walk(const Navigation::Grid& grid, const std::array<Move, Count>& moves) {
	Flood flood;
	const size_t cells = grid.cells.size();
	if (!cells || !grid.originKnown) return flood;
	flood.origin = Origin(grid);
	if (flood.origin >= cells) return flood;
	flood.parent.assign(cells, -2);
	flood.incoming.assign(cells, -1);
	flood.parent[flood.origin] = -1;
	flood.order.push_back(flood.origin);
	for (size_t cursor = 0; cursor < flood.order.size(); ++cursor) {
		const size_t index = flood.order[cursor];
		const int x = static_cast<int>(index % grid.width), y = static_cast<int>(index / grid.width);
		// Carry on in the direction this cell was reached from before trying any
		// other: among equally short routes that picks the one with the fewest
		// turns, which is also the one that survives the player taking a step.
		const size_t first = flood.incoming[index] >= 0 ? static_cast<size_t>(flood.incoming[index]) : 0;
		for (size_t step = 0; step < moves.size(); ++step) {
			const size_t m = (first + step) % moves.size();
			if (!(grid.cells[index].exits & moves[m].exit)) continue;
			const int nextX = x + moves[m].dx, nextY = y + moves[m].dy;
			if (nextX < 0 || nextY < 0 || nextX >= grid.width || nextY >= grid.height) continue;
			const size_t next = static_cast<size_t>(nextY) * grid.width + static_cast<size_t>(nextX);
			if (flood.parent[next] != -2) continue;
			flood.parent[next] = static_cast<int>(index);
			flood.incoming[next] = static_cast<int>(m);
			flood.order.push_back(next);
		}
	}
	flood.ok = true;
	return flood;
}

// Chebyshev distance from every cell to the nearest place the player has
// already walked. A target has to sit deep inside unexplored ground, or it is
// just the rim of the brush the player carries with him and it retreats at the
// same speed he approaches.
std::vector<int> DepthFromExplored(const Navigation::Grid& grid, const Explored::Map& explored) {
	std::vector<int> depth(grid.cells.size(), INT_MAX);
	std::vector<size_t> queue;
	for (size_t i = 0; i < grid.cells.size(); ++i) {
		const int x = grid.x + static_cast<int>(i % grid.width), y = grid.y + static_cast<int>(i / grid.width);
		if (!explored.Seen(x, y)) continue;
		depth[i] = 0;
		queue.push_back(i);
	}
	for (size_t cursor = 0; cursor < queue.size(); ++cursor) {
		const size_t index = queue[cursor];
		const int x = static_cast<int>(index % grid.width), y = static_cast<int>(index / grid.width);
		for (size_t n = 0; n < NeighbourX.size(); ++n) {
			const int nextX = x + NeighbourX[n], nextY = y + NeighbourY[n];
			if (nextX < 0 || nextY < 0 || nextX >= grid.width || nextY >= grid.height) continue;
			const size_t next = static_cast<size_t>(nextY) * grid.width + static_cast<size_t>(nextX);
			if (depth[next] != INT_MAX) continue;
			depth[next] = depth[index] + 1;
			queue.push_back(next);
		}
	}
	return depth;
}

Radar::Position CellPosition(const Navigation::Grid& grid, size_t index) {
	return {static_cast<double>(grid.x + static_cast<int>(index % grid.width)),
		static_cast<double>(grid.y + static_cast<int>(index / grid.width))};
}

bool Inside(const Navigation::Grid& grid, Radar::Position position, size_t& index) {
	const int x = static_cast<int>(std::lround(position.x)) - grid.x, y = static_cast<int>(std::lround(position.y)) - grid.y;
	if (x < 0 || y < 0 || x >= grid.width || y >= grid.height) return false;
	index = static_cast<size_t>(y) * grid.width + static_cast<size_t>(x);
	return true;
}

// Screen components of a world displacement: one unit of each is one push of
// the stick. Right is (+1,-1) and down is (+1,+1) in world cells, so
// (dx,dy) = right*(1,-1) + down*(1,1).
void Bearing(double dx, double dy, int& right, int& down) {
	right = static_cast<int>(std::lround((dx - dy) / 2));
	down = static_cast<int>(std::lround((dx + dy) / 2));
}

void Append(std::vector<Step>& steps, Radar::Direction direction) {
	if (!steps.empty() && steps.back().direction == direction) { ++steps.back().count; return; }
	steps.push_back({direction, 1});
}

// The walk from the player to `target`, as runs of identical stick pushes.
void Describe(const Navigation::Grid& grid, const Flood& flood, size_t target, const Settings& settings, Route& route) {
	route.outcome = Outcome::Route;
	route.target = CellPosition(grid, target);
	std::vector<Radar::Direction> reversed;
	for (size_t index = target; flood.parent[index] >= 0; index = static_cast<size_t>(flood.parent[index]))
		reversed.push_back(ScreenMoves[static_cast<size_t>(flood.incoming[index])].direction);
	route.distance = static_cast<int>(reversed.size());
	for (auto it = reversed.rbegin(); it != reversed.rend(); ++it) Append(route.steps, *it);
	if (settings.maxRuns && route.steps.size() > settings.maxRuns) {
		route.steps.resize(settings.maxRuns);
		route.truncated = true;
	}
}
}

Route Find(const Navigation::Grid& grid, const Explored::Map& explored, const Settings& settings, const Request& request) {
	Route route;
	if (grid.cells.empty() || !grid.originKnown) return route;
	const auto flood = Walk(grid, ScreenMoves);
	if (!flood.ok) return route;
	const auto depth = DepthFromExplored(grid, explored);

	// The target chosen on an earlier press stays the target: a route that
	// changes its mind every few steps cannot be walked. It is given up only
	// once it has been explored (which is what arriving does), or it is no
	// longer reachable.
	if (request.locked) {
		size_t index = 0;
		if (Inside(grid, *request.locked, index) && flood.parent[index] != -2) {
			if (depth[index] > 0) {
				Describe(grid, flood, index, settings, route);
				route.keptLocked = true;
				return route;
			}
			route.arrived = true; // walked onto it; the memory covers it now
		}
	}

	size_t deepest = 0, rim = 0;
	bool haveDeep = false, haveRim = false;
	for (const size_t index : flood.order) {
		if (index == flood.origin || depth[index] == 0) continue;
		if (!haveRim) { rim = index; haveRim = true; }
		if (depth[index] >= settings.probeRadius) { deepest = index; haveDeep = true; break; }
	}
	if (haveDeep || haveRim) {
		// Deep ground when there is any: it stays put while the player walks to
		// it. The rim is the endgame fallback, when a level is nearly swept and
		// only edges are left.
		Describe(grid, flood, haveDeep ? deepest : rim, settings, route);
		return route;
	}

	// Nothing unexplored along the stick's own directions. The world axes may
	// still reach some, and then the straight bearing is worth saying.
	const auto any = Walk(grid, AllMoves);
	size_t target = 0;
	bool found = false;
	if (any.ok)
		for (const size_t index : any.order) {
			if (index == any.origin || depth[index] == 0) continue;
			target = index;
			found = true;
			break;
		}
	if (!found) {
		route.outcome = Outcome::None;
		return route;
	}
	route.outcome = Outcome::Bearing;
	route.target = CellPosition(grid, target);
	int right = 0, down = 0;
	Bearing(route.target.x - grid.playerX, route.target.y - grid.playerY, right, down);
	route.distance = std::abs(right) + std::abs(down);
	if (down) route.steps.push_back({down > 0 ? Radar::Direction::Down : Radar::Direction::Up, std::abs(down)});
	if (right) route.steps.push_back({right > 0 ? Radar::Direction::Right : Radar::Direction::Left, std::abs(right)});
	return route;
}

std::string Describe(const Route& route, const Strings::Table& strings) {
	if (route.outcome == Outcome::Unavailable) return strings.exploreUnavailable;
	if (route.outcome == Outcome::None) return strings.exploreNone;
	std::string text = strings.exploreNearest;
	if (route.steps.empty()) {
		text += ", ";
		text += strings.scanAdjacent;
		return text;
	}
	char buffer[96];
	for (const auto& step : route.steps) {
		std::snprintf(buffer, sizeof buffer, strings.exploreStep,
			strings.radarMovementDirections[MovementIndex(step.direction)], step.count);
		text += ", ";
		text += buffer;
	}
	if (route.outcome == Outcome::Bearing) {
		text += ", ";
		text += strings.exploreNoRoute;
	}
	return text;
}
}
