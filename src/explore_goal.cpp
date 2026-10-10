#include "explore_goal.h"

#include <algorithm>
#include <cmath>

namespace D2RAccess::ExploreGoal {
namespace {
// Squared distance from a point to a room, zero inside it.
double DistanceSquared(const NativeRooms::Bounds& bounds, Position point) {
	const double dx = point.x < bounds.minX ? bounds.minX - point.x : (point.x >= bounds.maxX ? point.x - (bounds.maxX - 1) : 0);
	const double dy = point.y < bounds.minY ? bounds.minY - point.y : (point.y >= bounds.maxY ? point.y - (bounds.maxY - 1) : 0);
	return dx * dx + dy * dy;
}
}

bool Coverage::Seen(int x, int y) const {
	return x >= x_ && y >= y_ && x < x_ + width_ && y < y_ + height_
		&& cells_[static_cast<size_t>(y - y_) * width_ + x - x_] != 0;
}

void Coverage::Observe(const LevelRoute::Memory& memory, const Navigation::Grid& grid) {
	if (memory.X() != x_ || memory.Y() != y_ || memory.Width() != width_ || memory.Height() != height_) {
		std::vector<uint8_t> grown(static_cast<size_t>(memory.Width()) * memory.Height());
		for (int y = std::max(y_, memory.Y()); y < std::min(y_ + height_, memory.Y() + memory.Height()); ++y)
			for (int x = std::max(x_, memory.X()); x < std::min(x_ + width_, memory.X() + memory.Width()); ++x)
				grown[static_cast<size_t>(y - memory.Y()) * memory.Width() + x - memory.X()] = Seen(x, y);
		x_ = memory.X(); y_ = memory.Y(); width_ = memory.Width(); height_ = memory.Height();
		cells_ = std::move(grown);
	}
	// Twelve walking cells stays inside the scanner's fifteen-cell range.
	// Path distance, rather than a circle, leaves ground across walls unseen.
	for (int y = std::max(y_, grid.y); y < std::min(y_ + height_, grid.y + grid.height); ++y)
		for (int x = std::max(x_, grid.x); x < std::min(x_ + width_, grid.x + grid.width); ++x) {
			const auto* cell = grid.At(x, y);
			if (!cell || cell->body != Radar::State::Open || cell->steps < 0 || cell->steps > 12) continue;
			const int distance = std::max(std::abs(x - grid.playerX), std::abs(y - grid.playerY));
			bool visible = true;
			for (int step = 1; step <= distance; ++step) {
				const int sx = grid.playerX + static_cast<int>(std::lround(static_cast<double>(x - grid.playerX) * step / distance));
				const int sy = grid.playerY + static_cast<int>(std::lround(static_cast<double>(y - grid.playerY) * step / distance));
				const auto* through = grid.At(sx, sy);
				if (!through || through->body != Radar::State::Open) { visible = false; break; }
			}
			if (visible) cells_[static_cast<size_t>(y - y_) * width_ + x - x_] = 1;
		}
	const Position at{static_cast<double>(grid.playerX), static_cast<double>(grid.playerY)};
	if (revision_ != memory.Revision() || std::hypot(at.x - rejectedFrom_.x, at.y - rejectedFrom_.y) >= 8) {
		rejected_.clear(); revision_ = memory.Revision(); rejectedFrom_ = at;
	}
}

void Coverage::Reject(Position position, uint64_t revision) {
	if (revision_ != revision) rejected_.clear();
	revision_ = revision;
	rejected_.push_back(position);
}

bool Coverage::Rejected(int x, int y, uint64_t revision) const {
	return revision == revision_ && std::any_of(rejected_.begin(), rejected_.end(), [&](Position at) {
		return std::abs(at.x - x) <= 2 && std::abs(at.y - y) <= 2;
	});
}

bool Reached(const Coverage& coverage, Position player, Position target, bool doorApproach) {
	if (doorApproach) return std::hypot(player.x - target.x, player.y - target.y) <= 2;
	return coverage.Seen(static_cast<int>(std::lround(target.x)), static_cast<int>(std::lround(target.y)));
}

Choice Next(const LevelRoute::Memory& memory, const Coverage& coverage, Position player, int bodyPattern) {
	Choice choice;
	if (!memory.Revision()) return choice;
	const LevelRoute::Field known(memory, player, 1, bodyPattern, false);
	int nearest = -1;
	bool retry = true;
	for (int y = memory.Y(); y < memory.Y() + memory.Height(); ++y)
		for (int x = memory.X(); x < memory.X() + memory.Width(); ++x) {
			if (memory.At(x, y) != LevelRoute::Ground::Open || coverage.Seen(x, y)) continue;
			const int steps = known.Distance(x, y);
			const bool rejected = coverage.Rejected(x, y, memory.Revision());
			if (steps < 0 || (choice.found && !retry && rejected) || (choice.found && retry == rejected && steps >= nearest)) continue;
			retry = rejected;
			nearest = steps; choice.found = true; choice.position = {static_cast<double>(x), static_cast<double>(y)};
		}
	if (choice.found && !retry) return choice;
	const Choice deferred = choice; // a failed floor point must not hide an unloaded branch
	choice = {};
	// Find a route over ground still unloaded and through openable doors. This
	// is a search for where to stream next, never an instruction through a wall.
	const LevelRoute::Field future(memory, player, 1, bodyPattern, true, true);
	nearest = -1;
	retry = true;
	for (int y = memory.Y(); y < memory.Y() + memory.Height(); ++y)
		for (int x = memory.X(); x < memory.X() + memory.Width(); ++x) {
			const auto ground = memory.At(x, y);
			if ((ground != LevelRoute::Ground::Unseen && ground != LevelRoute::Ground::Open) || coverage.Seen(x, y)) continue;
			const int steps = future.Distance(x, y);
			const bool rejected = coverage.Rejected(x, y, memory.Revision());
			if (steps <= 1 || (choice.found && !retry && rejected) || (choice.found && retry == rejected && steps >= nearest)) continue;
			retry = rejected;
			nearest = steps; choice.found = true; choice.position = {static_cast<double>(x), static_cast<double>(y)};
		}
	if (choice.found) {
		auto path = future.From(choice.position); // target to player; inspect in travel order
		path.insert(path.begin(), choice.position); // From omits its origin, whose footprint may touch the door
		for (auto it = path.rbegin(); it != path.rend(); ++it) {
			const int x = static_cast<int>(it->x), y = static_cast<int>(it->y);
			for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
				if (memory.At(x + dx, y + dy) == LevelRoute::Ground::Door) {
					choice.position = {static_cast<double>(x + dx), static_cast<double>(y + dy)};
					choice.door = true;
					return choice;
				}
		}
	} else if (deferred.found) return deferred;
	else choice.complete = true; // no reachable floor or streaming frontier remains
	return choice;
}

Choice Nearest(const RoomGraph::Graph& graph, const Navigation::Grid& grid,
	const std::vector<NativeRooms::Room>& explored, Position player, const std::vector<uint64_t>& refused, const LevelRoute::Reach* reach) {
	Choice choice;
	const size_t from = NativeRooms::Containing(graph.rooms, player);
	if (from == NativeRooms::None) return choice;
	// Hops are the same in both directions, so one search from the player's
	// room measures every room of the level.
	const auto hops = RoomGraph::Distances(graph, from, reach ? nullptr : &grid);
	bool remaining = false;
	double nearest = 0;
	int bestSteps = -1;
	for (size_t room = 0; room < graph.rooms.size(); ++room) {
		if (room == from) continue;
		const auto id = graph.rooms[room].id;
		if (std::any_of(explored.begin(), explored.end(), [id](const auto& known) { return known.id == id; })) continue;
		remaining = true;
		if (hops[room] == RoomGraph::NoRoute || std::find(refused.begin(), refused.end(), id) != refused.end()) continue;
		const auto bounds = NativeRooms::Subtiles(graph.rooms[room]);
		if (reach && reach->Within(bounds) == LevelRoute::Reach::Answer::CutOff) continue;
		const double distance = DistanceSquared(bounds, player);
		Position entry{(bounds.minX + bounds.maxX - 1) / 2.0, (bounds.minY + bounds.maxY - 1) / 2.0};
		// Enter the room at reachable floor, not its geometric centre: town
		// rectangles include water, walls and narrow paths. When it is beyond
		// the loaded collision window, the existing room/map guidance leads.
		int steps = -1;
		for (int y = std::max(bounds.minY, grid.y); y < std::min(bounds.maxY, grid.y + grid.height); ++y)
			for (int x = std::max(bounds.minX, grid.x); x < std::min(bounds.maxX, grid.x + grid.width); ++x) {
				const auto* cell = grid.At(x, y);
				if (!cell || cell->body != Radar::State::Open || cell->steps < 0 || (steps >= 0 && cell->steps >= steps)) continue;
				steps = cell->steps;
				entry = {static_cast<double>(x), static_cast<double>(y)};
			}
		// A proved walking route comes before a guess through unloaded scenery.
		// Among proved routes choose the nearest entry by actual walking steps.
		if (choice.found) {
			if (bestSteps >= 0 && (steps < 0 || steps >= bestSteps)) continue;
			if (bestSteps < 0 && steps < 0
				&& (hops[room] > choice.hops || (hops[room] == choice.hops && distance >= nearest))) continue;
		}
		choice = {true, room, entry, hops[room]};
		bestSteps = steps;
		nearest = distance;
	}
	choice.complete = !remaining;
	return choice;
}
}
