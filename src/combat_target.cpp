#include "combat_target.h"

#include "auto_walk.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace D2RAccess::CombatTarget {
namespace {
int Cell(double value) { return static_cast<int>(std::lround(value)); }

// Walking steps to a monster: its own cell carries its collision, so the
// count is the fewest steps to any cell beside it, plus the one onto it.
int Steps(const Navigation::Grid& grid, Position position) {
	int fewest = -1;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			const auto* cell = grid.At(Cell(position.x) + dx, Cell(position.y) + dy);
			if (!cell || cell->steps < 0) continue;
			const int steps = cell->steps + (dx || dy ? 1 : 0);
			if (fewest < 0 || steps < fewest) fewest = steps;
		}
	}
	return fewest;
}

size_t Find(const std::vector<Candidate>& ranked, uint32_t id) {
	for (size_t i = 0; i < ranked.size(); ++i)
		if (ranked[i].entity.id == id) return i;
	return ranked.size();
}
}

std::vector<Candidate> Rank(const Radar::World& world, const Navigation::Grid& grid,
	const std::vector<EntityRadar::Entity>& entities, const Settings& settings) {
	std::vector<Candidate> ranked;
	for (const auto& entity : entities) {
		if (entity.kind != EntityRadar::Kind::Enemy || !entity.id) continue;
		Candidate candidate;
		candidate.distance = std::hypot(entity.position.x - world.player.x, entity.position.y - world.player.y);
		if (candidate.distance > settings.range) continue;
		candidate.steps = Steps(grid, entity.position);
		candidate.sight = EntityRadar::LineOfSight(world, world.player, entity.position);
		candidate.entity = entity;
		ranked.push_back(std::move(candidate));
	}
	std::stable_sort(ranked.begin(), ranked.end(), [](const Candidate& a, const Candidate& b) {
		if ((a.steps >= 0) != (b.steps >= 0)) return a.steps >= 0;
		if (a.steps >= 0 && a.steps != b.steps) return a.steps < b.steps;
		if (a.distance != b.distance) return a.distance < b.distance;
		return a.entity.id < b.entity.id; // two at the same distance keep one order
	});
	return ranked;
}

const Candidate* Lock::Choose(const std::vector<Candidate>& ranked, int delta) {
	if (ranked.empty()) { id_ = 0; return nullptr; }
	size_t index = 0;
	if (const size_t held = Find(ranked, id_); delta != 0 && held < ranked.size())
		index = (held + ranked.size() + (delta > 0 ? 1 : -1)) % ranked.size();
	id_ = ranked[index].entity.id;
	return &ranked[index];
}

const Candidate* Lock::Keep(const std::vector<Candidate>& ranked, bool& moved) {
	moved = false;
	if (ranked.empty()) { id_ = 0; return nullptr; }
	size_t index = Find(ranked, id_);
	if (index == ranked.size()) {
		index = 0;
		moved = true;
		id_ = ranked[index].entity.id;
	}
	return &ranked[index];
}

Move Decide(const Candidate& target, const Reach& reach) {
	const bool inReach = target.distance <= reach.range && (!reach.ranged || target.sight);
	if (inReach) return Move::Strike;
	return target.steps >= 0 ? Move::Close : Move::Unreachable;
}

double Clearance(Position from, const std::vector<Position>& threats) {
	double nearest = std::numeric_limits<double>::max();
	for (const auto threat : threats)
		nearest = std::min(nearest, std::hypot(threat.x - from.x, threat.y - from.y));
	return nearest;
}

bool Closing(Position player, const std::vector<Position>& threats, const Spacing& spacing) {
	return Clearance(player, threats) < spacing.tooClose;
}

namespace {
// Whether the walk there leads away: no step of it brings a monster nearer
// than it is now. Ground on the far side of a monster is not fallen back to.
bool Away(Position from, Position to, const std::vector<Position>& threats, double here) {
	const int steps = static_cast<int>(std::ceil(std::hypot(to.x - from.x, to.y - from.y)));
	for (int i = 1; i <= steps; ++i) {
		const double part = static_cast<double>(i) / steps;
		if (Clearance({from.x + (to.x - from.x) * part, from.y + (to.y - from.y) * part}, threats) < here) return false;
	}
	return true;
}
}

Ground FallBack(const Radar::World& world, const Navigation::Grid& grid, const std::vector<Position>& threats,
	Position target, double range, const Spacing& spacing) {
	Ground ground;
	if (threats.empty() || grid.cells.empty()) return ground;
	const double here = Clearance(world.player, threats);
	struct Place { Position cell; double clearance; int steps; };
	std::vector<Place> places;
	const int px = Cell(world.player.x), py = Cell(world.player.y);
	for (int y = py - spacing.leg; y <= py + spacing.leg; ++y) {
		for (int x = px - spacing.leg; x <= px + spacing.leg; ++x) {
			const auto* cell = grid.At(x, y);
			if (!cell || cell->steps < 1 || cell->steps > spacing.leg) continue;
			const Position place{static_cast<double>(x), static_cast<double>(y)};
			if (std::hypot(target.x - place.x, target.y - place.y) > range) continue;
			const double clearance = Clearance(place, threats);
			if (clearance < here + spacing.gain) continue;
			places.push_back({place, clearance, cell->steps});
		}
	}
	// The most distance first, up to what is enough; then the shortest walk.
	std::sort(places.begin(), places.end(), [&spacing](const Place& a, const Place& b) {
		const double ca = std::min(a.clearance, spacing.enough), cb = std::min(b.clearance, spacing.enough);
		if (ca != cb) return ca > cb;
		if (a.steps != b.steps) return a.steps < b.steps;
		return a.cell.y != b.cell.y ? a.cell.y < b.cell.y : a.cell.x < b.cell.x;
	});
	size_t viewed = 0;
	for (const auto& place : places) {
		// The cheap tests first; only a place that passes them is worth a look at the view.
		if (!Away(world.player, place.cell, threats, here) || !AutoWalk::Straight(world, world.player, place.cell)) continue;
		if (viewed++ >= spacing.views) break;
		if (!EntityRadar::LineOfSight(world, place.cell, target)) continue;
		ground.found = true;
		ground.cell = place.cell;
		ground.clearance = place.clearance;
		return ground;
	}
	return ground;
}
}
