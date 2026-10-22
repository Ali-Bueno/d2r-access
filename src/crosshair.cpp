#include "crosshair.h"

#include <algorithm>

namespace D2RAccess::Crosshair {
namespace {
constexpr uint32_t MonsterUnit = 1; // the game's unit type, as NativeHover::Target holds it
}

std::optional<Lock> Tracker::Update(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities,
	std::optional<Aim> aim, uint64_t nowMs, const Settings& settings) {
	const EntityRadar::Entity* target = nullptr;
	if (aim && aim->type == MonsterUnit) {
		const auto found = std::find_if(entities.begin(), entities.end(), [&](const EntityRadar::Entity& entity) {
			return entity.kind == EntityRadar::Kind::Enemy && entity.id == aim->id;
		});
		// The game aims through walls; like every automatic cue, this one keeps to what is in sight (docs/target-sight.md).
		if (found != entities.end() && EntityRadar::LineOfSight(world, world.player, found->position)) target = &*found;
	}
	std::erase_if(heard_, [&](const Heard& heard) { return nowMs - heard.ms >= settings.repeatMs; });
	if (!target) return std::nullopt;
	// Held, or back after a hop: quiet, and the target still counts as heard.
	const auto heard = std::find_if(heard_.begin(), heard_.end(), [&](const Heard& entry) { return entry.id == target->id; });
	if (heard != heard_.end()) { heard->ms = nowMs; return std::nullopt; }
	if (lastMs_ && nowMs - *lastMs_ < settings.gapMs) return std::nullopt;
	heard_.push_back({target->id, nowMs});
	lastMs_ = nowMs;
	return Lock{target->id, EntityCues::BearingOf(world.player, target->position)};
}
}
