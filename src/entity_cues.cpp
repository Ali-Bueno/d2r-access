#include "entity_cues.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace D2RAccess::EntityCues {
namespace {
constexpr size_t MaxEnemyClasses = 2; // leave room for combat, speech and the navigation radar
constexpr std::array Order{Kind::Important, Kind::Resurrector, Kind::Ranged, Kind::Npc};
std::optional<Kind> Classify(const EntityRadar::Entity& entity, const Settings& settings) {
	if (entity.kind == EntityRadar::Kind::Npc) return settings.npcs ? std::optional{Kind::Npc} : std::nullopt;
	if (entity.kind != EntityRadar::Kind::Enemy || !settings.enemies) return std::nullopt;
	if (entity.enemy.important) return Kind::Important;
	if (entity.enemy.resurrector) return Kind::Resurrector;
	if (entity.enemy.ranged && settings.ranged) return Kind::Ranged;
	return std::nullopt;
}
}

std::vector<Cue> Select(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities, const Settings& settings) {
	if (!std::isfinite(world.player.x) || !std::isfinite(world.player.y)
		|| !std::isfinite(settings.range) || settings.range <= 0) return {};
	std::array<std::optional<Cue>, 4> nearest;
	for (const auto& entity : entities) {
		const auto kind = Classify(entity, settings);
		if (!kind) continue;
		const double dx = entity.position.x - world.player.x, dy = entity.position.y - world.player.y;
		const double distance = std::hypot(dx, dy);
		if (!std::isfinite(distance) || distance > settings.range) continue;
		auto& slot = nearest[static_cast<size_t>(*kind)];
		if (slot && (distance > slot->distance || (distance == slot->distance && entity.id >= slot->id))) continue;
		if (!EntityRadar::LineOfSight(world, world.player, entity.position)) continue;
		Cue cue{entity.id, *kind, distance};
		if (distance > 0) {
			// Native isometric projection, shared with beacon and ground-item audio.
			cue.right = (dx / distance - dy / distance) * std::numbers::sqrt2 / 2;
			cue.front = -(dx / distance + dy / distance) * std::numbers::sqrt2 / 2;
		}
		const auto nearMs = std::max(settings.nearIntervalMs, settings.gapMs);
		const auto farMs = std::max(settings.farIntervalMs, nearMs);
		cue.intervalMs = nearMs + static_cast<uint64_t>(static_cast<double>(farMs - nearMs) * (distance / settings.range));
		slot = cue;
	}
	std::vector<Cue> selected;
	size_t enemies = 0;
	for (const auto kind : Order) {
		const auto& cue = nearest[static_cast<size_t>(kind)];
		if (!cue) continue;
		if (kind != Kind::Npc && enemies++ >= MaxEnemyClasses) continue;
		selected.push_back(*cue);
	}
	return selected;
}

std::optional<Cue> Tracker::Update(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities,
	uint64_t now, const Settings& settings) {
	if (played_ && now < last_) Reset();
	const auto selected = Select(world, entities, settings);
	std::array<bool, 4> present{};
	for (const auto& cue : selected) present[static_cast<size_t>(cue.kind)] = true;
	for (size_t i = 0; i < slots_.size(); ++i) {
		if (!present[i]) slots_[i] = {};
	}
	if (played_ && now - last_ < settings.gapMs) return std::nullopt;
	std::optional<Cue> next;
	double mostOverdue = -1;
	for (const auto& cue : selected) {
		const auto& slot = slots_[static_cast<size_t>(cue.kind)];
		const auto elapsed = now - slot.last;
		if (slot.played && elapsed < cue.intervalMs) continue;
		// Pending first cues run before repeats. Among repeats, overdue fraction
		// prevents a nearby boss from starving the other selected classes.
		const double overdue = slot.played ? static_cast<double>(elapsed) / std::max<uint64_t>(1, cue.intervalMs)
			: std::numeric_limits<double>::infinity();
		if (!next || overdue > mostOverdue) { next = cue; mostOverdue = overdue; }
	}
	if (next) {
		auto& slot = slots_[static_cast<size_t>(next->kind)];
		slot.played = true; slot.last = now;
		played_ = true; last_ = now;
	}
	return next;
}
void Tracker::Reset() { slots_ = {}; played_ = false; last_ = 0; }
}
