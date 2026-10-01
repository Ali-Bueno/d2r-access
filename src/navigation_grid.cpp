#include "navigation_grid.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace D2RAccess::Navigation {
namespace {
struct Delta { int x, y; };
// Native movement trace 0x364700 allows diagonal destination steps, including
// point-body corner cuts. World::Body supplies the actual footprint restriction.
constexpr std::array<Delta,8> Steps{{{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,-1},{-1,1},{1,1}}};
// The eight neighbours of one cell, in Steps' order; -1 past the window. The
// cell's column and row are found once here: finding them again for each
// neighbour was sixteen divisions a cell, in three passes over the grid.
std::array<int,8> Around(const Grid& grid, size_t index) {
	const auto at = static_cast<uint32_t>(index), width = static_cast<uint32_t>(grid.width);
	const int column = static_cast<int>(at % width), row = static_cast<int>(at / width);
	std::array<int,8> around{};
	for (size_t d=0; d<Steps.size(); ++d) {
		const int x = column + Steps[d].x, y = row + Steps[d].y;
		around[d] = x < 0 || y < 0 || x >= grid.width || y >= grid.height ? -1 : y*grid.width+x;
	}
	return around;
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
	for (int y=0; y<grid.height; ++y) for (int x=0; x<grid.width; ++x) {
		const size_t i = static_cast<size_t>(y)*grid.width+x;
		if (grid.cells[i].clearance && (x == 0 || y == 0 || x == grid.width-1 || y == grid.height-1)) {
			grid.cells[i].clearance = 1; queue.push_back(i);
		}
	}
	for (size_t cursor=0; cursor<queue.size(); ++cursor) {
		const size_t index = queue[cursor];
		const auto around = Around(grid,index);
		for (size_t d=0; d<Steps.size(); ++d) {
			const int next = around[d];
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
		const auto around = Around(grid,index);
		for (size_t d=0; d<Steps.size(); ++d) {
			const int next = around[d];
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
Grid Build(const Radar::World& world, int radius, Radar::Layer layer, bool clearance) {
	Grid grid;
	// Bound this research query by the existing radar's maximum default range.
	// Native positions are unsigned 16-bit cells, validated by ReadPlayer.
	constexpr int MaxPosition = std::numeric_limits<uint16_t>::max();
	static_assert(MaxRadius == static_cast<int>(Radar::Settings{}.farRange),
		"the grid window is the radar's far range; move the two together");
	if (radius < 1 || radius > MaxRadius || !std::isfinite(world.player.x)
		|| !std::isfinite(world.player.y) || world.player.x < 0 || world.player.y < 0
		|| world.player.x > MaxPosition || world.player.y > MaxPosition) return grid;
	grid.playerX = static_cast<int>(world.player.x); grid.playerY = static_cast<int>(world.player.y);
	grid.x = grid.playerX-radius; grid.y = grid.playerY-radius;
	grid.width = grid.height = radius*2+1;
	grid.session = world.session; grid.level = world.level;
	grid.bodyPattern = world.bodyPattern; grid.walkingMask = world.walkingMask;
	grid.cells.resize(static_cast<size_t>(grid.width)*grid.height);
	// Each room's rectangle is copied once into a flat field one cell wider
	// than the window, and the body is tested there. World::Cell searches the
	// room list on every call, and a cross body made that six searches a cell:
	// 20-38 ms a build at the full radius over 16-64 rooms (measured offline
	// 2026-10-01), on the game's own thread. The result is the same cell for
	// cell; the navigation-grid test holds the two against each other.
	const int fieldX = grid.x-1, fieldY = grid.y-1, fieldWidth = grid.width+2, fieldHeight = grid.height+2;
	const size_t fieldCells = static_cast<size_t>(fieldWidth)*fieldHeight;
	std::vector<uint16_t> raw(fieldCells, 0);
	std::vector<Radar::State> state(fieldCells, Radar::State::Unknown);
	std::vector<uint8_t> covered(fieldCells, 0); // the first room covering a cell owns it, as World::RoomAt has it
	for (const auto& room : world.rooms) {
		const int x0 = std::max(fieldX, room.x), x1 = std::min(fieldX+fieldWidth, room.x+room.width);
		const int y0 = std::max(fieldY, room.y), y1 = std::min(fieldY+fieldHeight, room.y+room.height);
		for (int y=y0; y<y1; ++y) for (int x=x0; x<x1; ++x) {
			const size_t index = static_cast<size_t>(y-fieldY)*fieldWidth+(x-fieldX);
			if (covered[index]) continue;
			covered[index] = 1;
			const size_t source = static_cast<size_t>(y-room.y)*room.width+(x-room.x);
			if (source >= room.cells.size() || (room.cells[source] & Radar::Collision::Blank)) continue;
			const uint16_t flags = room.cells[source];
			raw[index] = flags;
			const uint16_t tested = layer == Radar::Layer::Terrain ? Radar::TerrainCollisionFlags(flags) : flags;
			state[index] = tested & world.walkingMask ? Radar::State::Blocked : Radar::State::Open;
		}
	}
	for (int y=0; y<grid.height; ++y) for (int x=0; x<grid.width; ++x) {
		auto& cell = grid.cells[static_cast<size_t>(y)*grid.width+x];
		const size_t centre = static_cast<size_t>(y+1)*fieldWidth+(x+1);
		cell.flags = cell.blocker = raw[centre];
		cell.body = state[centre];
		// World::Body's own order: the first blocked cell of the footprint
		// names the blocker, an unknown one leaves the body unknown.
		for (int dy=-1; dy<=1 && cell.body != Radar::State::Blocked; ++dy) for (int dx=-1; dx<=1; ++dx) {
			if (world.bodyPattern == 0 || (dx == 0 && dy == 0)) continue;
			if ((world.bodyPattern % 2) != 0 && dx != 0 && dy != 0) continue;
			const size_t next = static_cast<size_t>(y+1+dy)*fieldWidth+(x+1+dx);
			if (state[next] == Radar::State::Blocked) { cell.blocker = raw[next]; cell.body = Radar::State::Blocked; break; }
			if (state[next] == Radar::State::Unknown) cell.body = Radar::State::Unknown;
		}
	}
	const size_t origin = static_cast<size_t>(radius)*grid.width+radius;
	for (size_t i=0; i<grid.cells.size(); ++i) {
		auto& cell = grid.cells[i];
		// Native trace skips the origin: permit escape from a known overlap,
		// but never traverse another blocked centre or return into this one.
		if (cell.body != Radar::State::Open && !(i == origin && cell.body == Radar::State::Blocked)) continue;
		const auto around = Around(grid,i);
		for (size_t d=0; d<Steps.size(); ++d) {
			const int next = around[d];
			if (next >= 0 && grid.cells[next].body == Radar::State::Open) cell.exits |= static_cast<uint8_t>(1u<<d);
		}
	}
	if (clearance) Clearance(grid);
	Reachability(grid,origin);
	return grid;
}
void LabelRooms(Grid& grid, const Radar::World& rooms) {
	for (int y=0; y<grid.height; ++y) for (int x=0; x<grid.width; ++x)
		grid.cells[static_cast<size_t>(y)*grid.width+x].room = rooms.RoomAt(grid.x+x, grid.y+y);
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
