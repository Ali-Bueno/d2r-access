#include "attack_reach.h"

#include <algorithm>
#include <cmath>

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

std::optional<Target> Nearest(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities, double reach) {
	if (!std::isfinite(reach) || reach <= 0) return std::nullopt;
	std::optional<Target> nearest;
	for (const auto& entity : entities) {
		if (entity.kind != EntityRadar::Kind::Enemy) continue;
		const auto bearing = EntityCues::BearingOf(world.player, entity.position);
		if (!std::isfinite(bearing.distance) || bearing.distance > reach) continue;
		if (nearest && (bearing.distance > nearest->bearing.distance
			|| (bearing.distance == nearest->bearing.distance && entity.id >= nearest->id))) continue;
		if (!EntityRadar::LineOfSight(world, world.player, entity.position)) continue;
		nearest = Target{entity.id, bearing};
	}
	return nearest;
}

std::optional<Target> Tracker::Update(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities,
	double reach, double release) {
	const auto inside = Nearest(world, entities, reach);
	if (inside) {
		const bool entered = !inReach_;
		inReach_ = true;
		return entered ? inside : std::nullopt;
	}
	// Staying in reach needs only the wider release distance, so an enemy
	// pacing at the edge does not chime on every step.
	if (inReach_ && !Nearest(world, entities, reach + std::max(0.0, release))) inReach_ = false;
	return std::nullopt;
}
}
