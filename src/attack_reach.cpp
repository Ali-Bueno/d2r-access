#include "attack_reach.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <tuple>

namespace D2RAccess::AttackReach {
namespace {
bool Any(const Loadout& loadout, SkillRange range, bool damageOnly = false) {
	return std::any_of(loadout.skills.begin(), loadout.skills.end(), [&](const Skill& skill) {
		return skill.range == range && (!damageOnly || skill.damage);
	});
}
}

// Strongest evidence first: a bow, a skill the game marks ranged, one it marks
// hand-to-hand, a spell that deals its own damage, then the weapon-bound Attack.
Style Classify(const Loadout& loadout) {
	if (loadout.missileWeapon || Any(loadout, SkillRange::Ranged)) return Style::Ranged;
	if (Any(loadout, SkillRange::HandToHand)) return Style::Melee;
	if (Any(loadout, SkillRange::None, true)) return Style::Ranged;
	return Style::Melee;
}

Style Resolve(Mode mode, const Loadout& loadout) {
	if (mode == Mode::Melee) return Style::Melee;
	if (mode == Mode::Ranged) return Style::Ranged;
	return Classify(loadout);
}

double Reach(Style style, const Loadout& loadout, const Settings& settings) {
	if (style == Style::Ranged) return settings.rangedReach;
	return settings.meleeReach + std::max(0, loadout.rangeAdder);
}

// D2R 3.3.93787, READ in 0x325200 and byte-identical to D2MOO's table: the
// distance by [dy][dx] between two units standing close, before the size corrections.
constexpr int Near = 8;
constexpr int NearDistance[Near][Near] = {
	{-1, -1, -1, 0, 2, 4, 6, 8}, {-1, -1, 0, 1, 2, 4, 6, 8}, {-1, 0, 0, 2, 3, 5, 7, 8}, {0, 1, 2, 2, 4, 5, 7, 8},
	{2, 2, 3, 4, 5, 6, 7, 9}, {4, 4, 5, 5, 6, 7, 8, 9}, {6, 6, 7, 7, 7, 8, 10, 10}, {8, 8, 8, 8, 9, 9, 10, 11},
};
constexpr int LargeSize = 4, MediumSize = 3, SmallSize = 1;

int UnitDistance(int dx, int dy, int sizeA, int sizeB) {
	dx = std::abs(dx); dy = std::abs(dy);
	if (dx < Near && dy < Near && sizeA < LargeSize && sizeB < LargeSize) {
		int distance = NearDistance[dy][dx];
		if (distance < 0) return 0;
		if (sizeA == MediumSize || sizeB == MediumSize) distance = std::max(distance - 1, 0);
		if (sizeA <= SmallSize || sizeB <= SmallSize) ++distance;
		return distance;
	}
	const int reach = sizeA / 2 + sizeB / 2;
	const int x = std::max(dx - reach, 0), y = std::max(dy - reach, 0);
	return 2 * std::max(x, y) + std::min(x, y);
}

int MeleeRange(const Loadout& loadout, std::optional<uint32_t> playerClass) {
	return std::max(0, loadout.rangeAdder) + (playerClass == WarlockClass ? WarlockMeleeBonus : 0);
}

bool InMeleeRange(int distance, int meleeRange, int bonus) {
	return distance <= 0 || distance <= meleeRange + 1 + bonus;
}

std::optional<Target> Nearest(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities,
	const Contact& contact, int extra) {
	const auto cell = [](double value) { return static_cast<int>(std::floor(value)); };
	std::optional<Target> nearest;
	for (const auto& entity : entities) {
		if (entity.kind != EntityRadar::Kind::Enemy) continue;
		const int distance = UnitDistance(cell(entity.position.x) - cell(world.player.x),
			cell(entity.position.y) - cell(world.player.y), PlayerSize, MonsterSize);
		if (!InMeleeRange(distance, contact.meleeRange + extra)) continue;
		const auto bearing = EntityCues::BearingOf(world.player, entity.position);
		if (nearest && std::tie(distance, bearing.distance, entity.id)
			>= std::tie(nearest->distance, nearest->bearing.distance, nearest->id)) continue;
		if (!EntityRadar::LineOfSight(world, world.player, entity.position)) continue;
		nearest = Target{entity.id, bearing, distance};
	}
	return nearest;
}
std::optional<Target> Tracker::Update(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities,
	const Contact& contact) {
	const auto inside = Nearest(world, entities, contact);
	if (inside) {
		const bool entered = !inReach_;
		inReach_ = true;
		return entered ? inside : std::nullopt;
	}
	// Staying in reach needs only the wider release distance, so an enemy
	// pacing at the edge does not chime on every step.
	if (inReach_ && !Nearest(world, entities, contact, std::max(0, contact.release))) inReach_ = false;
	return std::nullopt;
}
}
