#include "retreat.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <queue>

namespace D2RAccess::Retreat {
namespace {
// Navigation::DirectionBits order: a cell's `exits` bit d allows Steps[d].
struct Step { int x, y; bool diagonal; };
constexpr std::array<Step, 8> Steps{{{-1, 0, false}, {1, 0, false}, {0, -1, false}, {0, 1, false},
	{-1, -1, true}, {1, -1, true}, {-1, 1, true}, {1, 1, true}}};
constexpr int64_t Unreached = std::numeric_limits<int64_t>::max();

int Cell(double value) { return static_cast<int>(std::lround(value)); }

// What the monsters add to each cell of the grid.
std::vector<int> Danger(const Navigation::Grid& grid, const std::vector<Position>& threats, const Settings& settings) {
	std::vector<int> danger(grid.cells.size(), 0);
	double widest = 0;
	for (const auto& tier : settings.danger) widest = std::max(widest, tier.within);
	const int reach = static_cast<int>(std::ceil(widest));
	for (const auto& threat : threats) {
		const int cx = Cell(threat.x) - grid.x, cy = Cell(threat.y) - grid.y;
		for (int y = std::max(0, cy - reach); y <= std::min(grid.height - 1, cy + reach); ++y) {
			for (int x = std::max(0, cx - reach); x <= std::min(grid.width - 1, cx + reach); ++x) {
				const double distance = std::hypot(static_cast<double>(x - cx), static_cast<double>(y - cy));
				int cost = 0;
				for (const auto& tier : settings.danger)
					if (distance <= tier.within) cost = std::max(cost, tier.cost);
				danger[static_cast<size_t>(y) * grid.width + x] += cost;
			}
		}
	}
	return danger;
}
}

Plan Find(const Navigation::Grid& grid, const std::vector<Position>& goals,
	const std::vector<Position>& threats, const Settings& settings) {
	Plan plan;
	if (grid.cells.empty() || !grid.originKnown || goals.empty()) return plan;
	const auto danger = Danger(grid, threats, settings);
	const int diagonalCost = static_cast<int>(std::lround(settings.stepCost * std::numbers::sqrt2));
	const size_t origin = static_cast<size_t>(grid.playerY - grid.y) * grid.width + (grid.playerX - grid.x);
	std::vector<int64_t> cost(grid.cells.size(), Unreached);
	std::vector<int32_t> parent(grid.cells.size(), -1);
	using Entry = std::pair<int64_t, size_t>;
	std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
	cost[origin] = 0;
	open.push({0, origin});
	while (!open.empty()) {
		const auto [reached, index] = open.top();
		open.pop();
		if (reached != cost[index]) continue; // a cheaper way here was found since
		const int x = static_cast<int>(index % grid.width), y = static_cast<int>(index / grid.width);
		for (size_t d = 0; d < Steps.size(); ++d) {
			if (!(grid.cells[index].exits & (1u << d))) continue;
			const int nx = x + Steps[d].x, ny = y + Steps[d].y;
			if (nx < 0 || ny < 0 || nx >= grid.width || ny >= grid.height) continue;
			const size_t next = static_cast<size_t>(ny) * grid.width + nx;
			const int64_t total = reached + (Steps[d].diagonal ? diagonalCost : settings.stepCost) + danger[next];
			if (total >= cost[next]) continue;
			cost[next] = total;
			parent[next] = static_cast<int32_t>(index);
			open.push({total, next});
		}
	}
	// A way out is reached from any cell beside it; the cheapest such cell of
	// any way out is where the retreat ends.
	const int near = static_cast<int>(std::ceil(settings.arrival));
	size_t end = origin;
	int64_t best = Unreached;
	for (size_t goal = 0; goal < goals.size(); ++goal) {
		const int gx = Cell(goals[goal].x) - grid.x, gy = Cell(goals[goal].y) - grid.y;
		for (int y = std::max(0, gy - near); y <= std::min(grid.height - 1, gy + near); ++y) {
			for (int x = std::max(0, gx - near); x <= std::min(grid.width - 1, gx + near); ++x) {
				if (std::hypot(static_cast<double>(x - gx), static_cast<double>(y - gy)) > settings.arrival) continue;
				const size_t index = static_cast<size_t>(y) * grid.width + x;
				if (cost[index] >= best) continue;
				best = cost[index];
				end = index;
				plan.goal = goal;
			}
		}
	}
	if (best == Unreached) return plan;
	plan.found = true;
	const int blow = settings.danger.front().cost;
	for (size_t index = end; index != origin; index = static_cast<size_t>(parent[index])) {
		plan.path.push_back({static_cast<double>(grid.x + static_cast<int>(index % grid.width)),
			static_cast<double>(grid.y + static_cast<int>(index / grid.width))});
		plan.threatened |= danger[index] >= blow;
	}
	std::reverse(plan.path.begin(), plan.path.end());
	Position previous{static_cast<double>(grid.playerX), static_cast<double>(grid.playerY)};
	for (const auto point : plan.path) {
		plan.length += std::hypot(point.x - previous.x, point.y - previous.y);
		previous = point;
	}
	return plan;
}

namespace {
// Open cells from a cell along a direction, itself excluded, up to `limit`;
// `walled` tells a run ended by a wall from one ended by the limit, the edge
// of the grid or ground not read yet.
int Run(const Navigation::Grid& grid, int x, int y, int dx, int dy, int limit, bool& walled) {
	walled = false;
	for (int run = 0; run < limit; ++run) {
		x += dx;
		y += dy;
		const auto* cell = grid.At(x, y);
		if (!cell || cell->body == Radar::State::Unknown) return run;
		if (cell->body != Radar::State::Open) { walled = true; return run; }
	}
	return limit;
}
}

std::vector<Position> Chokepoints(const Navigation::Grid& grid, const Narrows& narrows) {
	std::vector<Position> points;
	// Across a passage, and along it: the world axes, then the diagonals.
	constexpr int Axes[4][4] = {{1, 0, 0, 1}, {0, 1, 1, 0}, {1, 1, 1, -1}, {1, -1, 1, 1}};
	for (int row = 0; row < grid.height; ++row) {
		for (int column = 0; column < grid.width; ++column) {
			const auto& cell = grid.cells[static_cast<size_t>(row) * grid.width + column];
			if (cell.steps < 0 || cell.body != Radar::State::Open) continue;
			const int x = grid.x + column, y = grid.y + row;
			for (const auto& axis : Axes) {
				bool wallA = false, wallB = false, unused = false;
				const int across = Run(grid, x, y, axis[0], axis[1], narrows.width, wallA)
					+ Run(grid, x, y, -axis[0], -axis[1], narrows.width, wallB) + 1;
				if (!wallA || !wallB || across > narrows.width) continue;
				if (Run(grid, x, y, axis[2], axis[3], narrows.through, unused) < narrows.through
					|| Run(grid, x, y, -axis[2], -axis[3], narrows.through, unused) < narrows.through) continue;
				points.push_back({static_cast<double>(x), static_cast<double>(y)});
				break;
			}
		}
	}
	return points;
}
}
