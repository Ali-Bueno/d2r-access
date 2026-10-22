#include "level_route.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
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
			if (Radar::InBody(bodyPattern, dx, dy)) body.push_back(dy * row + dx);
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

bool Memory::Extend(const std::vector<NativeRooms::Room>& rooms) {
	if (cells_.empty() || rooms.empty()) return false;
	int minX = x_, minY = y_, maxX = x_ + width_, maxY = y_ + height_;
	for (const auto& room : rooms) {
		const auto bounds = NativeRooms::Subtiles(room);
		minX = std::min(minX, bounds.minX); minY = std::min(minY, bounds.minY);
		maxX = std::max(maxX, bounds.maxX); maxY = std::max(maxY, bounds.maxY);
	}
	const int64_t width = int64_t(maxX) - minX, height = int64_t(maxY) - minY;
	if (width <= 0 || height <= 0 || width * height > MaxCells) return false; // fail closed, as Begin does
	bool grew = false;
	if (minX != x_ || minY != y_ || width != width_ || height != height_) {
		// What was learnt is kept: the old grid is copied into its place in the new one.
		std::vector<Ground> grown(static_cast<size_t>(width * height), Ground::Outside);
		for (int y = 0; y < height_; ++y)
			std::copy_n(cells_.begin() + static_cast<size_t>(y) * width_, width_,
				grown.begin() + static_cast<size_t>(y + y_ - minY) * static_cast<size_t>(width) + (x_ - minX));
		cells_ = std::move(grown);
		x_ = minX; y_ = minY;
		width_ = static_cast<int>(width); height_ = static_cast<int>(height);
		grew = true;
	}
	// A room the bounds already covered can still be outside the level as this
	// map has it, when it was built after Begin: its ground becomes unseen, which
	// a way may cross as a guess. Learnt ground is never put back to unseen.
	for (const auto& room : rooms) {
		const auto bounds = NativeRooms::Subtiles(room);
		for (int y = std::max(y_, bounds.minY); y < std::min(y_ + height_, bounds.maxY); ++y)
			for (int x = std::max(x_, bounds.minX); x < std::min(x_ + width_, bounds.maxX); ++x) {
				auto& cell = cells_[static_cast<size_t>(y - y_) * width_ + (x - x_)];
				if (cell != Ground::Outside) continue;
				cell = Ground::Unseen;
				grew = true;
			}
	}
	if (grew) ++revision_;
	return grew;
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
				// (docs/collision-semantics.md), or OBJECT alone beside a door noted
				// (NoteDoor); OBJECT alone elsewhere is a chest or a barrel.
				const bool door = (shut & Radar::Collision::Door) || (shut == Radar::Collision::Object && NearDoor(x, y));
				const Ground ground = (raw & Radar::Collision::Blank) || (shut & ~DoorBits) ? Ground::Blocked
					: door ? Ground::Door : shut ? Ground::Blocked : Ground::Open;
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

bool Memory::NoteDoor(Position at, int reach) {
	const NotedDoor door{Cell(at.x), Cell(at.y), reach};
	if (cells_.empty() || std::any_of(doors_.begin(), doors_.end(),
		[&](const NotedDoor& known) { return known.x == door.x && known.y == door.y; })) return false;
	doors_.push_back(door);
	return true;
}

bool Memory::NearDoor(int x, int y) const {
	return std::any_of(doors_.begin(), doors_.end(),
		[&](const NotedDoor& door) { return std::abs(x - door.x) <= door.reach && std::abs(y - door.y) <= door.reach; });
}

Area Area::Around(Position place, int margin) {
	const int x = Cell(place.x), y = Cell(place.y);
	return {x - margin, y - margin, x + margin, y + margin, true};
}

Area Area::Of(const NativeRooms::Room& room) {
	const auto bounds = NativeRooms::Subtiles(room);
	return {bounds.minX, bounds.minY, bounds.maxX - 1, bounds.maxY - 1};
}

bool Area::Contains(Position at) const {
	const int x = Cell(at.x), y = Cell(at.y);
	return x >= minX && x <= maxX && y >= minY && y <= maxY;
}

Position Area::Centre() const { return {(minX + maxX) / 2.0, (minY + maxY) / 2.0}; }

Arrival::Arrival(const Memory& memory, const Area& area) : area_(area) {
	if (!area.walked || area.maxX < area.minX || area.maxY < area.minY) return;
	const int width = area.maxX - area.minX + 1, height = area.maxY - area.minY + 1;
	counts_.assign(static_cast<size_t>(width) * height, 0);
	const auto at = [&](int x, int y) { return static_cast<size_t>(y - area.minY) * width + static_cast<size_t>(x - area.minX); };
	const auto walkable = [&](int x, int y) {
		if (x < area.minX || y < area.minY || x > area.maxX || y > area.maxY) return false;
		const Ground ground = memory.At(x, y);
		return ground == Ground::Open || ground == Ground::Unseen;
	};
	const int cx = (area.minX + area.maxX) / 2, cy = (area.minY + area.maxY) / 2;
	const int reach = std::min(std::min(cx - area.minX, area.maxX - cx), std::min(cy - area.minY, area.maxY - cy));
	std::vector<std::pair<int, int>> ring, next;
	for (int radius = 0; ring.empty() && radius <= reach; ++radius)
		for (int y = cy - radius; y <= cy + radius; ++y)
			for (int x = cx - radius; x <= cx + radius; ++x)
				if (std::max(std::abs(x - cx), std::abs(y - cy)) == radius && walkable(x, y)) {
					counts_[at(x, y)] = 1;
					ring.emplace_back(x, y);
				}
	for (int steps = 0; steps < reach && !ring.empty(); ++steps) {
		next.clear();
		for (const auto [x, y] : ring) {
			for (const auto step : Steps8) {
				const int nx = x + step.x, ny = y + step.y;
				if (!walkable(nx, ny) || counts_[at(nx, ny)]) continue;
				if (step.x && step.y && (!walkable(nx, y) || !walkable(x, ny))) continue;
				counts_[at(nx, ny)] = 1;
				next.emplace_back(nx, ny);
			}
		}
		ring.swap(next);
	}
}

bool Arrival::Contains(int x, int y) const {
	if (x < area_.minX || y < area_.minY || x > area_.maxX || y > area_.maxY) return false;
	if (counts_.empty()) return !area_.walked;
	return counts_[static_cast<size_t>(y - area_.minY) * (area_.maxX - area_.minX + 1) + static_cast<size_t>(x - area_.minX)] != 0;
}

Memory Memory::Copy(const std::vector<NativeRooms::Room>* rooms, uint64_t revision) const {
	Memory copy = *this;
	copy.revision_ = revision;
	if (!rooms || cells_.empty()) return copy;
	// What the rooms kept do not cover is the level still, only not known.
	std::replace_if(copy.cells_.begin(), copy.cells_.end(), [](Ground ground) { return ground != Ground::Outside; }, Ground::Unseen);
	for (const auto& room : *rooms) {
		const auto bounds = NativeRooms::Subtiles(room);
		const int x0 = std::max(bounds.minX, x_), x1 = std::min(bounds.maxX, x_ + width_);
		for (int y = std::max(bounds.minY, y_); y < std::min(bounds.maxY, y_ + height_) && x0 < x1; ++y) {
			const size_t row = static_cast<size_t>(y - y_) * width_;
			std::copy(cells_.begin() + row + (x0 - x_), cells_.begin() + row + (x1 - x_), copy.cells_.begin() + row + (x0 - x_));
		}
	}
	return copy;
}

Ground Memory::At(int x, int y) const {
	const int64_t dx = int64_t(x) - x_, dy = int64_t(y) - y_;
	if (dx < 0 || dy < 0 || dx >= width_ || dy >= height_) return Ground::Outside;
	return cells_[static_cast<size_t>(dy) * width_ + static_cast<size_t>(dx)];
}

Field::Field(const Memory& memory, Position place, int margin, int bodyPattern, bool unseen, bool doors)
	: Field(memory, Area::Around(place, margin), bodyPattern, unseen, doors) {}

Field::Field(const Memory& memory, const Area& goal, int bodyPattern, bool unseen, bool doors)
	: x_(memory.X()), y_(memory.Y()), width_(memory.Width()), height_(memory.Height()) {
	if (width_ <= 0 || height_ <= 0) return;
	// Unless asked, a shut door is no part of a way: the game will not walk through one.
	const Floor floor = Standing(memory, bodyPattern, unseen, doors);
	const size_t across = floor.across;
	const auto& stood = floor.stood;
	steps_.assign(stood.size(), -1);
	const auto index = [&](int x, int y) { return static_cast<size_t>(y - y_ + 1) * across + static_cast<size_t>(x - x_ + 1); };
	const auto row = static_cast<int64_t>(across);
	std::vector<uint32_t> queue;
	const Arrival arrival(memory, goal);
	for (int y = std::max(goal.minY, y_); y <= std::min(goal.maxY, y_ + height_ - 1); ++y) {
		for (int x = std::max(goal.minX, x_); x <= std::min(goal.maxX, x_ + width_ - 1); ++x) {
			if (!stood[index(x, y)] || !arrival.Contains(x, y)) continue;
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

Way Plan::From(const Memory& memory, Position from, Position place, int margin, int bodyPattern, bool doors) {
	return From(memory, from, Area::Around(place, margin), bodyPattern, doors);
}

Way Plan::From(const Memory& memory, Position from, const Area& goal, int bodyPattern, bool doors) {
	const Key key{memory.Session(), memory.Revision(), memory.Level(), bodyPattern, goal, doors};
	if (!(key == key_)) {
		key_ = key;
		seen_.reset();
		any_.reset();
	}
	Way way;
	if (!memory.Revision()) return way;
	if (!seen_) seen_.emplace(memory, goal, bodyPattern, false, doors);
	way.path = seen_->From(from);
	if (!way.path.empty() || seen_->At(from)) return way;
	if (!any_) any_.emplace(memory, goal, bodyPattern, true, doors);
	way.path = any_->From(from);
	way.guess = !way.path.empty();
	return way;
}

Reach::Reach(const Memory& memory, Position from, int bodyPattern)
	: session_(memory.Session()), revision_(memory.Revision()), level_(memory.Level()), pattern_(bodyPattern),
	  x_(memory.X()), y_(memory.Y()), width_(memory.Width()), height_(memory.Height()) {
	Flood(memory, from, bodyPattern, {}, 0);
}

Reach::Reach(const Memory& memory, Position from, int bodyPattern, const std::vector<Link>& links, int useWithin, uint64_t pads)
	: session_(memory.Session()), revision_(memory.Revision()), pads_(pads), level_(memory.Level()), pattern_(bodyPattern),
	  x_(memory.X()), y_(memory.Y()), width_(memory.Width()), height_(memory.Height()), linked_(true) {
	Flood(memory, from, bodyPattern, links, useWithin);
}

void Reach::Flood(const Memory& memory, Position from, int bodyPattern, const std::vector<Link>& links, int useWithin) {
	if (width_ <= 0 || height_ <= 0) return;
	const Floor floor = Standing(memory, bodyPattern, true, true);
	const size_t across = floor.across;
	cells_.resize(floor.stood.size());
	for (size_t i = 0; i < cells_.size(); ++i)
		cells_[i] = floor.ground[i] == Ground::Outside ? Outside : floor.stood[i] ? Stood : Shut;
	if (linked_) {
		steps_.assign(cells_.size(), -1);
		via_.assign(cells_.size(), 0);
		links_ = links;
	}
	const auto index = [&](int x, int y) { return static_cast<size_t>(y - y_ + 1) * across + static_cast<size_t>(x - x_ + 1); };
	std::vector<uint32_t> queue;
	const auto got = [&](size_t at, int32_t steps, uint16_t via) {
		cells_[at] = Got;
		if (linked_) { steps_[at] = steps; via_[at] = via; }
		queue.push_back(static_cast<uint32_t>(at));
	};
	const auto seed = [&](int x, int y) {
		if (At(x, y) != Stood) return;
		got(index(x, y), 0, 0);
	};
	fromX_ = Cell(from.x);
	fromY_ = Cell(from.y);
	const int x = fromX_, y = fromY_;
	seed(x, y);
	// The cell stood on need not fit the body (Field::From): the cells beside it that do.
	if (queue.empty())
		for (const auto step : Steps8) seed(x + step.x, y + step.y);
	any_ = !queue.empty();
	// The cells a pad is used from.
	std::vector<uint8_t> uses;
	if (!links.empty()) {
		uses.assign(cells_.size(), 0);
		for (const auto& link : links)
			for (int useY = link.from.y - useWithin; useY <= link.from.y + useWithin; ++useY)
				for (int useX = link.from.x - useWithin; useX <= link.from.x + useWithin; ++useX)
					if (At(useX, useY) != Outside) uses[index(useX, useY)] = 1;
	}
	std::vector<uint8_t> taken(links.size(), 0);
	const auto row = static_cast<int64_t>(across);
	std::array<int64_t, 8> around{};
	for (size_t i = 0; i < Steps8.size(); ++i) around[i] = Steps8[i].y * row + Steps8[i].x;
	for (size_t head = 0; head < queue.size(); ++head) {
		const size_t at = queue[head];
		if (!uses.empty() && uses[at]) {
			// A pad is used from here: on to the ground beside the one it leads to.
			const int atX = static_cast<int>(at % across) - 1 + x_, atY = static_cast<int>(at / across) - 1 + y_;
			for (size_t i = 0; i < links.size(); ++i) {
				const auto& link = links[i];
				if (taken[i] || std::abs(atX - link.from.x) > useWithin || std::abs(atY - link.from.y) > useWithin) continue;
				taken[i] = 1;
				const auto via = via_[at] ? via_[at] : static_cast<uint16_t>(i + 1);
				for (int landY = link.to.y - LandWithin; landY <= link.to.y + LandWithin; ++landY)
					for (int landX = link.to.x - LandWithin; landX <= link.to.x + LandWithin; ++landX)
						if (At(landX, landY) == Stood) got(index(landX, landY), steps_[at] + 1, via);
			}
		}
		for (const auto offset : around) {
			const auto next = static_cast<size_t>(static_cast<int64_t>(at) + offset);
			if (cells_[next] != Stood) continue;
			got(next, linked_ ? steps_[at] + 1 : 0, linked_ ? via_[at] : uint16_t{0});
		}
	}
}

Reach::Mark Reach::At(int x, int y) const {
	if (cells_.empty() || x < x_ || y < y_ || x >= x_ + width_ || y >= y_ + height_) return Outside;
	return static_cast<Mark>(cells_[static_cast<size_t>(y - y_ + 1) * (static_cast<size_t>(width_) + 2) + static_cast<size_t>(x - x_ + 1)]);
}

bool Reach::Holds(const Memory& memory, Position from, int bodyPattern, uint64_t pads) const {
	return linked_ && !cells_.empty() && pads == pads_ && Cell(from.x) == fromX_ && Cell(from.y) == fromY_
		&& memory.Session() == session_ && memory.Revision() == revision_ && memory.Level() == level_ && bodyPattern == pattern_;
}

bool Reach::Holds(const Memory& memory, Position from, int bodyPattern) const {
	if (linked_ || cells_.empty() || memory.Session() != session_ || memory.Revision() != revision_ || memory.Level() != level_
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

bool Reach::First(int minX, int minY, int maxX, int maxY, Pad& pad, int* steps) const {
	if (!linked_ || !any_) return false;
	int32_t fewest = -1;
	uint16_t via = 0;
	for (int y = std::max(minY, y_); y < std::min(maxY, y_ + height_); ++y) {
		for (int x = std::max(minX, x_); x < std::min(maxX, x_ + width_); ++x) {
			if (At(x, y) != Got) continue;
			const size_t at = static_cast<size_t>(y - y_ + 1) * (static_cast<size_t>(width_) + 2) + static_cast<size_t>(x - x_ + 1);
			if (fewest < 0 || steps_[at] < fewest) { fewest = steps_[at]; via = via_[at]; }
		}
	}
	if (!via) return false;
	pad = links_[via - 1].from;
	if (steps) *steps = fewest;
	return true;
}

bool Reach::Via(Position place, int margin, Pad& pad, int* steps) const {
	const int x = Cell(place.x), y = Cell(place.y);
	return First(x - margin, y - margin, x + margin + 1, y + margin + 1, pad, steps);
}

bool Reach::ViaWithin(const NativeRooms::Bounds& bounds, Pad& pad, int* steps) const {
	return First(bounds.minX, bounds.minY, bounds.maxX, bounds.maxY, pad, steps);
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
