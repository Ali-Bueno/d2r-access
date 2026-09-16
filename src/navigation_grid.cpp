#include "navigation_grid.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace D2RAccess::Navigation {
namespace {
struct Delta { int x, y; };
// Native movement trace 0x364700 allows diagonal destination steps, including
// point-body corner cuts. World::Body supplies the actual footprint restriction.
constexpr std::array<Delta,8> Steps{{{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,-1},{-1,1},{1,1}}};
int Neighbor(const Grid& grid, size_t index, size_t direction) {
	const int x = static_cast<int>(index % grid.width) + Steps[direction].x;
	const int y = static_cast<int>(index / grid.width) + Steps[direction].y;
	return x < 0 || y < 0 || x >= grid.width || y >= grid.height ? -1 : y*grid.width+x;
}
void Clearance(Grid& grid) {
	std::vector<size_t> queue;
	for (size_t i=0; i<grid.cells.size(); ++i) {
		auto& cell = grid.cells[i];
		cell.clearance = cell.body == Radar::State::Open ? grid.width+grid.height : 0;
		if (cell.clearance == 0) queue.push_back(i);
	}
	// The analysis window is a coverage limit, not evidence of a wall. Its
	// outer unknown cells also limit how much extra clearance can be proved.
	for (size_t i=0; i<grid.cells.size(); ++i) {
		const int x = static_cast<int>(i % grid.width), y = static_cast<int>(i / grid.width);
		if (grid.cells[i].clearance && (x == 0 || y == 0 || x == grid.width-1 || y == grid.height-1)) {
			grid.cells[i].clearance = 1; queue.push_back(i);
		}
	}
	for (size_t cursor=0; cursor<queue.size(); ++cursor) {
		const size_t index = queue[cursor];
		for (size_t d=0; d<Steps.size(); ++d) {
			const int next = Neighbor(grid,index,d);
			if (next < 0 || grid.cells[next].clearance <= grid.cells[index].clearance+1) continue;
			grid.cells[next].clearance = grid.cells[index].clearance+1;
			queue.push_back(static_cast<size_t>(next));
		}
	}
}
void Reachability(Grid& grid, size_t origin) {
	grid.originKnown = grid.cells[origin].body != Radar::State::Unknown;
	if (!grid.originKnown) return;
	std::vector<size_t> queue{origin};
	grid.cells[origin].steps = 0;
	for (size_t cursor=0; cursor<queue.size(); ++cursor) {
		const size_t index = queue[cursor];
		const auto& current = grid.cells[index];
		for (size_t d=0; d<Steps.size(); ++d) {
			const int next = Neighbor(grid,index,d);
			if (next < 0) { grid.touchesWindow = true; continue; }
			auto& cell = grid.cells[next];
			if (cell.body == Radar::State::Unknown) grid.touchesUnknown = true;
			if (!(current.exits & (1u<<d))) continue;
			const auto first = index == origin ? static_cast<uint8_t>(1u<<d) : current.firstSteps;
			if (cell.steps < 0) {
				cell.steps = current.steps+1; cell.firstSteps = first;
				queue.push_back(static_cast<size_t>(next));
			} else if (cell.steps == current.steps+1) {
				// All BFS parents at the previous distance are visited before
				// their children; retain every equally short initial direction.
				cell.firstSteps |= first;
			}
		}
	}
	grid.reachable = queue.size();
}
}
const Cell* Grid::At(int worldX, int worldY) const {
	const int64_t dx = int64_t(worldX)-x, dy = int64_t(worldY)-y;
	if (dx < 0 || dy < 0 || dx >= width || dy >= height) return nullptr;
	return &cells[static_cast<size_t>(dy*width+dx)];
}
Grid Build(const Radar::World& world, int radius) {
	Grid grid;
	// Bound this research query by the existing radar's maximum default range.
	// Native positions are unsigned 16-bit cells, validated by ReadPlayer.
	constexpr int MaxPosition = std::numeric_limits<uint16_t>::max();
	if (radius < 1 || radius > Radar::Settings{}.farRange || !std::isfinite(world.player.x)
		|| !std::isfinite(world.player.y) || world.player.x < 0 || world.player.y < 0
		|| world.player.x > MaxPosition || world.player.y > MaxPosition) return grid;
	grid.playerX = static_cast<int>(world.player.x); grid.playerY = static_cast<int>(world.player.y);
	grid.x = grid.playerX-radius; grid.y = grid.playerY-radius;
	grid.width = grid.height = radius*2+1;
	grid.session = world.session; grid.level = world.level;
	grid.bodyPattern = world.bodyPattern; grid.walkingMask = world.walkingMask;
	grid.cells.resize(static_cast<size_t>(grid.width)*grid.height);
	for (int y=0; y<grid.height; ++y) for (int x=0; x<grid.width; ++x) {
		auto& cell = grid.cells[static_cast<size_t>(y)*grid.width+x];
		(void)world.Cell(grid.x+x, grid.y+y, Radar::Layer::Movement, &cell.flags);
		cell.body = world.Body(grid.x+x, grid.y+y);
	}
	const size_t origin = static_cast<size_t>(radius)*grid.width+radius;
	for (size_t i=0; i<grid.cells.size(); ++i) {
		auto& cell = grid.cells[i];
		// Native trace skips the origin: permit escape from a known overlap,
		// but never traverse another blocked centre or return into this one.
		if (cell.body != Radar::State::Open && !(i == origin && cell.body == Radar::State::Blocked)) continue;
		for (size_t d=0; d<Steps.size(); ++d) {
			const int next = Neighbor(grid,i,d);
			if (next >= 0 && grid.cells[next].body == Radar::State::Open) cell.exits |= static_cast<uint8_t>(1u<<d);
		}
	}
	Clearance(grid);
	Reachability(grid,origin);
	return grid;
}
std::string Grid::Dump() const {
	std::string result;
	for (int row=0; row<height; ++row) {
		for (int column=0; column<width; ++column) {
			const auto& cell = cells[static_cast<size_t>(row)*width+column];
			result += x+column == playerX && y+row == playerY ? 'P' : cell.body == Radar::State::Unknown ? '?'
				: cell.body == Radar::State::Blocked ? '#' : cell.steps < 0 ? 'o' : '.';
		}
		result += '\n';
	}
	return result;
}
}
