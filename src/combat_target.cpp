#include "combat_target.h"

#include <algorithm>
#include <cmath>

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
}
