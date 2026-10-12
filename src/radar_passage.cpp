#include "radar_passage.h"
#include <algorithm>
#include <cmath>

namespace D2RAccess::Radar {
namespace {
// Native movement trace 0x364700 allows diagonal destination steps.
constexpr std::array<Position, 8> Steps{{{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,-1},{-1,1},{1,1}}};
const double MaxStep = std::hypot(1.0, 1.0);
}
double BodySpan(int bodyPattern) { return bodyPattern == 0 ? 1.0 : 3.0; }
int PassageRadius(double reachDepth, int bodyPattern) {
	return static_cast<int>(std::ceil(reachDepth + BodySpan(bodyPattern))) + 1;
}
Passage ScanPassage(const Navigation::Grid& grid, Position axis, const PassageQuery& query) {
	Passage result;
	result.openDepth = query.openDepth;
	result.closeDepth = query.closeDepth;
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
	// Rounding between two cells at the same raster offset from the axis.
	constexpr double RasterNoise = 1e-9;
	double obstacleAside = 0, obstacleAhead = 0;
	const int home = grid.At(grid.playerX, grid.playerY)->room;
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
				if (projection(next) <= query.openDepth) unknown = true;
				continue;
			}
			if (cell->body != State::Open) {
				// The obstacle the body pushes against within the closing depth
				// is a closed door when the game's own DOOR bit blocks it.
				const double ahead = projection(next), aside = std::abs(lateral(next));
				if (ahead > query.closeDepth + MaxStep) continue;
				if (cell->blocker & Collision::Door) result.door = true;
				// The one it meets head-on lies nearest the axis, then nearest the player.
				if (!result.obstacle || aside < obstacleAside - RasterNoise
					|| (aside <= obstacleAside + RasterNoise && ahead < obstacleAhead)) {
					result.obstacle = true;
					result.obstacleX = grid.playerX + static_cast<int>(next.x);
					result.obstacleY = grid.playerY + static_cast<int>(next.y);
					obstacleAside = aside;
					obstacleAhead = ahead;
				}
				continue;
			}
			if (visited[index(next)]) continue;
			visited[index(next)] = true;
			result.depth = std::max(result.depth, projection(next));
			if (home >= 0 && cell->room >= 0 && cell->room != home && (result.crossing < 0 || projection(next) < result.crossing)) {
				result.crossing = projection(next);
				result.crossingRoom = cell->room;
			}
			queue.push_back(next);
		}
	}
	// One threshold each way and no dead band: between them the committed
	// reading stands, so the first observation after a reset is a verdict too.
	if (result.depth >= query.openDepth) result.available = true;
	else result.available = !unknown;
	if (!result.available) return result;
	if (result.depth <= query.closeDepth) { result.observed = result.door ? State::Door : State::Blocked; return result; }
	if (result.depth < query.openDepth && Obstacle(query.committed)) { result.observed = query.committed; return result; }
	// What the passable route means: it arrives within a doorway of a level
	// exit, or it enters another chamber and clears its border by a doorway.
	if (query.exits) for (size_t i = 0; i < query.exits->size(); ++i) {
		const auto& exit = (*query.exits)[i].position;
		const Position relative{exit.x - grid.playerX, exit.y - grid.playerY};
		const double ahead = projection(relative), aside = std::abs(lateral(relative));
		if (ahead > 0 && ahead <= result.depth + query.wayThrough && aside <= query.wayThrough / 2) {
			result.observed = State::Exit;
			result.exit = static_cast<int>(i);
			return result;
		}
	}
	const bool leaves = query.maze && result.crossing >= 0 && result.depth >= result.crossing + query.wayThrough;
	result.observed = leaves ? State::Exit : State::Open;
	return result;
}
}
