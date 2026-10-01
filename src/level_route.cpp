#include "level_route.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace D2RAccess::LevelRoute {
namespace {
// The widest level is 200x200 tiles (Levels.txt SizeX/SizeY), a million cells.
constexpr int64_t MaxCells = 1024 * 1024;
// A unit's own centre marker. A live unit's is taken off with the unit's bits
// (Radar::TerrainCollisionFlags); a bare one is a marker left behind
// (radar.h LeftMarkers), and neither is floor to remember.
constexpr uint16_t NoPath = 0x1000;
struct Step { int x, y; };
// Straight steps first, so a way runs straight where it can.
constexpr std::array<Step, 8> Steps8{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}}};

int Cell(double value) { return static_cast<int>(std::lround(value)); }
}

bool Memory::Begin(uint64_t session, int level, const std::vector<NativeRooms::Room>& rooms) {
	*this = {};
	if (rooms.empty()) return false;
	int minX = std::numeric_limits<int>::max(), minY = minX, maxX = std::numeric_limits<int>::min(), maxY = maxX;
	for (const auto& room : rooms) {
		const auto bounds = NativeRooms::Subtiles(room);
		minX = std::min(minX, bounds.minX); minY = std::min(minY, bounds.minY);
		maxX = std::max(maxX, bounds.maxX); maxY = std::max(maxY, bounds.maxY);
	}
	const int64_t width = int64_t(maxX) - minX, height = int64_t(maxY) - minY;
	if (width <= 0 || height <= 0 || width * height > MaxCells) return false;
	session_ = session;
	level_ = level;
	x_ = minX; y_ = minY;
	width_ = static_cast<int>(width); height_ = static_cast<int>(height);
	cells_.assign(static_cast<size_t>(width * height), Ground::Outside);
	for (const auto& room : rooms) {
		const auto bounds = NativeRooms::Subtiles(room);
		for (int y = bounds.minY; y < bounds.maxY; ++y)
			std::fill_n(cells_.begin() + static_cast<size_t>(y - y_) * width_ + (bounds.minX - x_), bounds.maxX - bounds.minX, Ground::Unseen);
	}
	revision_ = 1;
	return true;
}

bool Memory::Learn(const Radar::World& world) {
	if (cells_.empty() || world.session != session_ || world.level != level_) return false;
	const uint16_t mask = static_cast<uint16_t>(world.walkingMask & ~NoPath);
	bool changed = false;
	// The first room covering a cell owns it (Radar::World::RoomAt): written last here.
	for (auto room = world.rooms.rbegin(); room != world.rooms.rend(); ++room) {
		const int x0 = std::max(x_, room->x), x1 = std::min(x_ + width_, room->x + room->width);
		const int y0 = std::max(y_, room->y), y1 = std::min(y_ + height_, room->y + room->height);
		for (int y = y0; y < y1; ++y) {
			for (int x = x0; x < x1; ++x) {
				const size_t source = static_cast<size_t>(y - room->y) * room->width + (x - room->x);
				if (source >= room->cells.size()) continue;
				const uint16_t raw = room->cells[source];
				// A blank cell of a loaded room is no floor, and no route here crosses
				// one (Beacon, Navigation): shut, or a way would be planned through it
				// for ever.
				const Ground ground = (raw & Radar::Collision::Blank) || (Radar::TerrainCollisionFlags(raw) & mask)
					? Ground::Blocked : Ground::Open;
				auto& cell = cells_[static_cast<size_t>(y - y_) * width_ + (x - x_)];
				if (cell == ground) continue;
				cell = ground;
				changed = true;
			}
		}
	}
	if (changed) ++revision_;
	return changed;
}

Ground Memory::At(int x, int y) const {
	const int64_t dx = int64_t(x) - x_, dy = int64_t(y) - y_;
	if (dx < 0 || dy < 0 || dx >= width_ || dy >= height_) return Ground::Outside;
	return cells_[static_cast<size_t>(dy) * width_ + static_cast<size_t>(dx)];
}

Field::Field(const Memory& memory, Position place, int margin, int bodyPattern, bool unseen)
	: x_(memory.X()), y_(memory.Y()), width_(memory.Width()), height_(memory.Height()) {
	if (width_ <= 0 || height_ <= 0) return;
	// One cell of margin all round, never stood on, so no step is tested against the level's edge.
	const size_t across = static_cast<size_t>(width_) + 2, cells = across * (static_cast<size_t>(height_) + 2);
	steps_.assign(cells, -1);
	const auto index = [&](int x, int y) { return static_cast<size_t>(y - y_ + 1) * across + static_cast<size_t>(x - x_ + 1); };
	const auto row = static_cast<int64_t>(across);
	std::vector<Ground> ground(cells, Ground::Outside);
	for (int y = 0; y < height_; ++y)
		std::copy_n(memory.Cells().begin() + static_cast<size_t>(y) * width_, width_, ground.begin() + static_cast<size_t>(y + 1) * across + 1);
	// The cells of the body round its centre.
	std::vector<int64_t> body;
	for (int dy = -1; dy <= 1; ++dy)
		for (int dx = -1; dx <= 1; ++dx)
			if ((dx || dy) && bodyPattern != 0 && ((bodyPattern % 2) == 0 || dx == 0 || dy == 0)) body.push_back(dy * row + dx);
	// Whether the body fits on each cell: on ground seen open, or with `unseen` on ground not seen yet.
	std::vector<uint8_t> stood(cells, 0);
	for (int y = y_; y < y_ + height_; ++y) {
		for (int x = x_; x < x_ + width_; ++x) {
			const size_t at = index(x, y);
			if (ground[at] != Ground::Open && ground[at] != Ground::Unseen) continue;
			bool fits = true, seen = ground[at] == Ground::Open;
			for (const auto offset : body) {
				const Ground beside = ground[static_cast<size_t>(static_cast<int64_t>(at) + offset)];
				// Past the level's own rooms the body cannot be shown to fit.
				if (beside == Ground::Outside || beside == Ground::Blocked) { fits = false; break; }
				if (beside == Ground::Unseen) seen = false;
			}
			stood[at] = fits && (seen || unseen);
		}
	}
	std::vector<uint32_t> queue;
	const int placeX = Cell(place.x), placeY = Cell(place.y);
	for (int dy = -margin; dy <= margin; ++dy) {
		for (int dx = -margin; dx <= margin; ++dx) {
			const int x = placeX + dx, y = placeY + dy;
			if (x < x_ || y < y_ || x >= x_ + width_ || y >= y_ + height_ || !stood[index(x, y)]) continue;
			steps_[index(x, y)] = 0;
			queue.push_back(static_cast<uint32_t>(index(x, y)));
		}
	}
	std::array<int64_t, 8> around{};
	for (size_t i = 0; i < Steps8.size(); ++i) around[i] = Steps8[i].y * row + Steps8[i].x;
	for (size_t head = 0; head < queue.size(); ++head) {
		const size_t at = queue[head];
		for (const auto offset : around) {
			const auto next = static_cast<size_t>(static_cast<int64_t>(at) + offset);
			if (steps_[next] >= 0 || !stood[next]) continue;
			steps_[next] = steps_[at] + 1;
			queue.push_back(static_cast<uint32_t>(next));
		}
	}
}

int Field::Steps(int x, int y) const {
	if (x < x_ || y < y_ || x >= x_ + width_ || y >= y_ + height_) return -1;
	return steps_[static_cast<size_t>(y - y_ + 1) * (static_cast<size_t>(width_) + 2) + static_cast<size_t>(x - x_ + 1)];
}

bool Field::At(Position from) const { return Steps(Cell(from.x), Cell(from.y)) == 0; }

std::vector<Position> Field::From(Position from) const {
	std::vector<Position> path;
	int x = Cell(from.x), y = Cell(from.y);
	int steps = Steps(x, y);
	if (steps == 0) return path;
	if (steps < 0) {
		// The cell stood on fails the body test: the nearest of its neighbours that has a way.
		int best = -1, bestX = 0, bestY = 0;
		for (const auto step : Steps8) {
			const int found = Steps(x + step.x, y + step.y);
			if (found >= 0 && (best < 0 || found < best)) { best = found; bestX = x + step.x; bestY = y + step.y; }
		}
		if (best < 0) return path;
		x = bestX; y = bestY; steps = best;
		path.push_back({static_cast<double>(x), static_cast<double>(y)});
	}
	path.reserve(path.size() + static_cast<size_t>(steps));
	while (steps > 0) {
		bool stepped = false;
		for (const auto step : Steps8) {
			if (Steps(x + step.x, y + step.y) != steps - 1) continue;
			x += step.x; y += step.y; --steps;
			path.push_back({static_cast<double>(x), static_cast<double>(y)});
			stepped = true;
			break;
		}
		if (!stepped) return {}; // cannot happen: every counted cell was reached from one a step nearer
	}
	return path;
}

Way Plan::From(const Memory& memory, Position from, Position place, int margin, int bodyPattern) {
	const Key key{memory.Session(), memory.Revision(), memory.Level(), Cell(place.x), Cell(place.y), margin, bodyPattern};
	if (!(key == key_)) {
		key_ = key;
		seen_.reset();
		any_.reset();
	}
	Way way;
	if (!memory.Revision()) return way;
	if (!seen_) seen_.emplace(memory, place, margin, bodyPattern, false);
	way.path = seen_->From(from);
	if (!way.path.empty() || seen_->At(from)) return way;
	if (!any_) any_.emplace(memory, place, margin, bodyPattern, true);
	way.path = any_->From(from);
	way.guess = !way.path.empty();
	return way;
}

bool Ahead(const std::vector<Position>& path, const Navigation::Grid& grid, Position& cell) {
	bool found = false;
	for (const auto point : path) {
		const auto* loaded = grid.At(Cell(point.x), Cell(point.y));
		if (!loaded || loaded->steps < 0) break;
		cell = point;
		found = true;
	}
	return found;
}
}
