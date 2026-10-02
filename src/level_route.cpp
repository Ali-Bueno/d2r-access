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
constexpr uint16_t DoorBits = Radar::Collision::Door | Radar::Collision::Object;
struct Step { int x, y; };
// Straight steps first, so a way runs straight where it can.
constexpr std::array<Step, 8> Steps8{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}}};

int Cell(double value) { return static_cast<int>(std::lround(value)); }

// The level with one cell of margin all round, never stood on, so no step is
// tested against the level's edge, and where on it the body fits: on ground
// seen open, with `unseen` on ground not seen yet, with `doors` in a doorway
// whose door is shut.
struct Floor {
	size_t across = 0;
	std::vector<Ground> ground;
	std::vector<uint8_t> stood;
};
Floor Standing(const Memory& memory, int bodyPattern, bool unseen, bool doors) {
	Floor floor;
	const int width = memory.Width(), height = memory.Height();
	floor.across = static_cast<size_t>(width) + 2;
	const size_t cells = floor.across * (static_cast<size_t>(height) + 2);
	floor.ground.assign(cells, Ground::Outside);
	for (int y = 0; y < height; ++y)
		std::copy_n(memory.Cells().begin() + static_cast<size_t>(y) * width, width, floor.ground.begin() + static_cast<size_t>(y + 1) * floor.across + 1);
	const auto row = static_cast<int64_t>(floor.across);
	// The cells of the body round its centre.
	std::vector<int64_t> body;
	for (int dy = -1; dy <= 1; ++dy)
		for (int dx = -1; dx <= 1; ++dx)
			if ((dx || dy) && bodyPattern != 0 && ((bodyPattern % 2) == 0 || dx == 0 || dy == 0)) body.push_back(dy * row + dx);
	const auto open = [doors](Ground ground) { return ground == Ground::Open || (doors && ground == Ground::Door); };
	floor.stood.assign(cells, 0);
	for (int y = 1; y <= height; ++y) {
		for (int x = 1; x <= width; ++x) {
			const size_t at = static_cast<size_t>(y) * floor.across + static_cast<size_t>(x);
			bool seen = open(floor.ground[at]);
			if (!seen && floor.ground[at] != Ground::Unseen) continue;
			bool fits = true;
			for (const auto offset : body) {
				const Ground beside = floor.ground[static_cast<size_t>(static_cast<int64_t>(at) + offset)];
				if (beside == Ground::Unseen) seen = false;
				// Past the level's own rooms the body cannot be shown to fit.
				else if (!open(beside)) { fits = false; break; }
			}
			floor.stood[at] = fits && (seen || unseen);
		}
	}
	return floor;
}
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
				const auto shut = static_cast<uint16_t>(Radar::TerrainCollisionFlags(raw) & mask);
				// A blank cell of a loaded room is no floor, and no route here crosses
				// one (Beacon, Navigation): shut, or a way would be planned through it
				// for ever. A shut door's cells carry DOOR, which opening it clears
				// (docs/collision-semantics.md); whether they carry OBJECT too is not
				// measured, so either counts as the door's own.
				const Ground ground = (raw & Radar::Collision::Blank) || (shut & ~DoorBits) ? Ground::Blocked
					: (shut & Radar::Collision::Door) ? Ground::Door : shut ? Ground::Blocked : Ground::Open;
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
	// A shut door is no part of a way: the game will not walk through one.
	const Floor floor = Standing(memory, bodyPattern, unseen, false);
	const size_t across = floor.across;
	const auto& stood = floor.stood;
	steps_.assign(stood.size(), -1);
	const auto index = [&](int x, int y) { return static_cast<size_t>(y - y_ + 1) * across + static_cast<size_t>(x - x_ + 1); };
	const auto row = static_cast<int64_t>(across);
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

Reach::Reach(const Memory& memory, Position from, int bodyPattern)
	: session_(memory.Session()), revision_(memory.Revision()), level_(memory.Level()), pattern_(bodyPattern),
	  x_(memory.X()), y_(memory.Y()), width_(memory.Width()), height_(memory.Height()) {
	if (width_ <= 0 || height_ <= 0) return;
	const Floor floor = Standing(memory, bodyPattern, true, true);
	const size_t across = floor.across;
	cells_.resize(floor.stood.size());
	for (size_t i = 0; i < cells_.size(); ++i)
		cells_[i] = floor.ground[i] == Ground::Outside ? Outside : floor.stood[i] ? Stood : Shut;
	std::vector<uint32_t> queue;
	const auto seed = [&](int x, int y) {
		if (At(x, y) != Stood) return;
		const size_t at = static_cast<size_t>(y - y_ + 1) * across + static_cast<size_t>(x - x_ + 1);
		cells_[at] = Got;
		queue.push_back(static_cast<uint32_t>(at));
	};
	const int x = Cell(from.x), y = Cell(from.y);
	seed(x, y);
	// The cell stood on need not fit the body (Field::From): the cells beside it that do.
	if (queue.empty())
		for (const auto step : Steps8) seed(x + step.x, y + step.y);
	any_ = !queue.empty();
	const auto row = static_cast<int64_t>(across);
	std::array<int64_t, 8> around{};
	for (size_t i = 0; i < Steps8.size(); ++i) around[i] = Steps8[i].y * row + Steps8[i].x;
	for (size_t head = 0; head < queue.size(); ++head) {
		for (const auto offset : around) {
			const auto next = static_cast<size_t>(static_cast<int64_t>(queue[head]) + offset);
			if (cells_[next] != Stood) continue;
			cells_[next] = Got;
			queue.push_back(static_cast<uint32_t>(next));
		}
	}
}

Reach::Mark Reach::At(int x, int y) const {
	if (cells_.empty() || x < x_ || y < y_ || x >= x_ + width_ || y >= y_ + height_) return Outside;
	return static_cast<Mark>(cells_[static_cast<size_t>(y - y_ + 1) * (static_cast<size_t>(width_) + 2) + static_cast<size_t>(x - x_ + 1)]);
}

bool Reach::Holds(const Memory& memory, Position from, int bodyPattern) const {
	if (cells_.empty() || memory.Session() != session_ || memory.Revision() != revision_ || memory.Level() != level_
		|| bodyPattern != pattern_) return false;
	const int x = Cell(from.x), y = Cell(from.y);
	const Mark mark = At(x, y);
	if (mark != Shut && mark != Outside) return mark == Got;
	// Where the body does not fit, every place beside it that it does fit was a start.
	bool beside = false;
	for (const auto step : Steps8) {
		const Mark next = At(x + step.x, y + step.y);
		if (next == Stood) return false;
		beside = beside || next == Got;
	}
	return beside;
}

Reach::Answer Reach::Among(int minX, int minY, int maxX, int maxY) const {
	if (!any_) return Answer::Unknown;
	bool inside = false;
	for (int y = std::max(minY, y_); y < std::min(maxY, y_ + height_); ++y) {
		for (int x = std::max(minX, x_); x < std::min(maxX, x_ + width_); ++x) {
			const Mark mark = At(x, y);
			if (mark == Got) return Answer::Reached;
			inside = inside || mark != Outside;
		}
	}
	return inside ? Answer::CutOff : Answer::Unknown;
}

Reach::Answer Reach::To(Position place, int margin) const {
	const int x = Cell(place.x), y = Cell(place.y);
	return Among(x - margin, y - margin, x + margin + 1, y + margin + 1);
}

Reach::Answer Reach::Within(const NativeRooms::Bounds& bounds) const {
	return Among(bounds.minX, bounds.minY, bounds.maxX, bounds.maxY);
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
