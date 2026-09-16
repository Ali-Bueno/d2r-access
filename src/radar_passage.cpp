#include "radar_passage.h"
#include <algorithm>
#include <cmath>

namespace D2RAccess::Radar {
namespace {
// Native movement trace 0x364700 allows diagonal destination steps.
constexpr std::array<Position, 8> Steps{{{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,-1},{-1,1},{1,1}}};
}
double BodySpan(int bodyPattern) { return bodyPattern == 0 ? 1.0 : 3.0; }
int PassageRadius(double reachDepth, int bodyPattern) {
	return static_cast<int>(std::ceil(reachDepth + BodySpan(bodyPattern))) + 1;
}
Passage ScanPassage(const Navigation::Grid& grid, Position axis, double openDepth, double closeDepth) {
	Passage result;
	result.openDepth = openDepth;
	result.closeDepth = closeDepth;
	if (!grid.originKnown) return result;
	const auto projection = [&](Position p) { return p.x*axis.x + p.y*axis.y; };
	const auto lateral = [&](Position p) { return -p.x*axis.y + p.y*axis.x; };
	const auto index = [&](Position p) {
		return static_cast<size_t>(grid.playerY+static_cast<int>(p.y)-grid.y)*grid.width
			+grid.playerX+static_cast<int>(p.x)-grid.x;
	};
	// One native cell of lateral tolerance: the staircase of a diagonal line
	// alternates component steps, each 0.707 off the axis, never further.
	constexpr double NativeCell = 1;
	std::vector<Position> queue{{}};
	std::vector<bool> visited(grid.cells.size());
	visited[index({})] = true;
	bool unknown = false;
	for (size_t head = 0; head < queue.size(); ++head) {
		const auto current = queue[head];
		for (const auto step : Steps) {
			// Never label a route requiring a retreat as a forward opening: the
			// first step must make progress along the stick axis (the diagonal
			// or either of its components, as the native move slides).
			if (projection(step) < 0 || (head == 0 && projection(step) <= 0)) continue;
			const Position next{current.x+step.x, current.y+step.y};
			if (std::abs(lateral(next)) > NativeCell) continue;
			const auto* cell = grid.At(grid.playerX+static_cast<int>(next.x), grid.playerY+static_cast<int>(next.y));
			if (!cell || cell->body == State::Unknown) {
				// Unloaded cells may hide the rest of the route; they are never
				// walls. Beyond the opening depth they only limit the reach.
				if (projection(next) <= openDepth) unknown = true;
				continue;
			}
			if (cell->body != State::Open || visited[index(next)]) continue;
			visited[index(next)] = true;
			result.depth = std::max(result.depth, projection(next));
			queue.push_back(next);
		}
	}
	if (result.depth >= openDepth) { result.available = true; result.observed = State::Open; return result; }
	result.available = !unknown;
	if (result.available && result.depth <= closeDepth) result.observed = State::Blocked;
	return result;
}
}
