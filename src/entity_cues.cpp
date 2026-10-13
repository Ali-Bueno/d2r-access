#include "entity_cues.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace D2RAccess::EntityCues {
namespace {
constexpr size_t MaxEnemyClasses = 2; // leave room for combat, speech and the navigation radar
constexpr std::array Priorities{Kind::Important, Kind::Resurrector, Kind::Ranged};
constexpr std::array Order{Kind::Important, Kind::Resurrector, Kind::Ranged, Kind::Npc};
std::optional<Kind> Classify(const EntityRadar::Entity& entity, const Settings& settings) {
	if (entity.kind == EntityRadar::Kind::Npc) return settings.npcs ? std::optional{Kind::Npc} : std::nullopt;
	if (entity.kind != EntityRadar::Kind::Enemy || !settings.enemies) return std::nullopt;
	if (entity.enemy.important) return Kind::Important;
	if (entity.enemy.resurrector) return Kind::Resurrector;
	if (entity.enemy.ranged && settings.ranged) return Kind::Ranged;
	return settings.ordinary ? std::optional{Kind::Ordinary} : std::nullopt;
}
// The distance at which pacing reaches its slowest: the farthest point of the screen, or the range.
double Extent(const Settings& settings) {
	if (!settings.onScreen) return settings.range;
	const auto& screen = settings.screen;
	return std::hypot((screen.halfWidth + screen.halfHeight) / 2, (screen.halfHeight - screen.halfWidth) / 2) + RangedMargin;
}
bool InArea(Radar::Position player, Radar::Position at, double distance, Kind kind, const Settings& settings) {
	if (!settings.onScreen) return distance <= settings.range;
	auto screen = settings.screen;
	if (kind == Kind::Ranged) { screen.halfWidth += RangedMargin; screen.halfHeight += RangedMargin; }
	return CombatTarget::OnScreen(player, at, screen);
}
uint64_t IntervalMs(double distance, const Settings& settings) {
	const auto nearMs = std::max(settings.nearIntervalMs, settings.gapMs);
	const auto farMs = std::max(settings.farIntervalMs, nearMs);
	return nearMs + static_cast<uint64_t>(static_cast<double>(farMs - nearMs) * std::min(1.0, distance / Extent(settings)));
}
bool Nearer(const Cue& a, const Cue& b) { return a.distance < b.distance || (a.distance == b.distance && a.id < b.id); }
}

Bearing BearingOf(Radar::Position player, Radar::Position target) {
	const double dx = target.x - player.x, dy = target.y - player.y;
	Bearing bearing{std::hypot(dx, dy)};
	if (bearing.distance > 0 && std::isfinite(bearing.distance)) {
		// Native isometric projection, shared with beacon and ground-item audio.
		bearing.right = (dx / bearing.distance - dy / bearing.distance) * std::numbers::sqrt2 / 2;
		bearing.front = -(dx / bearing.distance + dy / bearing.distance) * std::numbers::sqrt2 / 2;
	}
	return bearing;
}

static std::vector<Cue> LegacySelect(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities, const Settings& settings) {
	if (!std::isfinite(world.player.x) || !std::isfinite(world.player.y)
		|| !std::isfinite(settings.range) || settings.range <= 0) return {};
	std::array<std::optional<Cue>, 4> nearest;
	for (const auto& entity : entities) {
		const auto kind = Classify(entity, settings);
		if (!kind) continue;
		const double dx = entity.position.x - world.player.x, dy = entity.position.y - world.player.y;
		const double distance = std::hypot(dx, dy);
		if (!std::isfinite(distance) || !InArea(world.player, entity.position, distance, *kind, settings)) continue;
		auto& slot = nearest[static_cast<size_t>(*kind)];
		if (slot && (distance > slot->distance || (distance == slot->distance && entity.id >= slot->id))) continue;
		if (!EntityRadar::LineOfSight(world, world.player, entity.position)) continue;
		Cue cue{entity.id, *kind, entity.enemy.boss, distance};
		cue.slot = static_cast<size_t>(*kind);
		if (distance > 0) {
			// Native isometric projection, shared with beacon and ground-item audio.
			cue.right = (dx / distance - dy / distance) * std::numbers::sqrt2 / 2;
			cue.front = -(dx / distance + dy / distance) * std::numbers::sqrt2 / 2;
		}
		cue.intervalMs = IntervalMs(distance, settings);
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

std::vector<Cue> Select(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities, const Settings& settings) {
	if (!std::isfinite(world.player.x) || !std::isfinite(world.player.y)
		|| !std::isfinite(settings.range) || settings.range <= 0) return {};
	if (!settings.ordinary) return LegacySelect(world, entities, settings);
	const size_t maxVoices = std::clamp<size_t>(settings.maxVoices, 1, MaxVoices);
	std::array<std::optional<Cue>, KindCount> nearest;
	struct Candidate { Cue cue; Radar::Position position; };
	std::vector<Candidate> ordinary;
	for (const auto& entity : entities) {
		const auto kind = Classify(entity, settings);
		if (!kind) continue;
		const auto bearing = BearingOf(world.player, entity.position);
		const double distance = bearing.distance;
		if (!std::isfinite(distance) || !InArea(world.player, entity.position, distance, *kind, settings)) continue;
		Cue cue{entity.id, *kind, entity.enemy.boss, distance, bearing.right, bearing.front};
		cue.intervalMs = IntervalMs(distance, settings);
		cue.slot = static_cast<size_t>(*kind);
		// Ordinary enemies are ranked first and only the nearest few pay for a terrain ray.
		if (*kind == Kind::Ordinary) { ordinary.push_back({cue, entity.position}); continue; }
		auto& slot = nearest[cue.slot];
		if (slot && !Nearer(cue, *slot)) continue;
		if (!EntityRadar::LineOfSight(world, world.player, entity.position)) continue;
		slot = cue;
	}
	std::vector<Cue> selected;
	for (const auto kind : Priorities)
		if (const auto& cue = nearest[static_cast<size_t>(kind)]; cue && selected.size() < maxVoices) selected.push_back(*cue);
	std::sort(ordinary.begin(), ordinary.end(), [](const Candidate& a, const Candidate& b) { return Nearer(a.cue, b.cue); });
	// One slot per rank, so the nearest enemy changing does not restart its cadence.
	size_t rank = 0;
	for (auto& candidate : ordinary) {
		if (selected.size() >= maxVoices || rank >= MaxOrdinary) break;
		if (!EntityRadar::LineOfSight(world, world.player, candidate.position)) continue;
		candidate.cue.slot = static_cast<size_t>(Kind::Ordinary) + rank++;
		selected.push_back(candidate.cue);
	}
	if (nearest[static_cast<size_t>(Kind::Npc)]) selected.push_back(*nearest[static_cast<size_t>(Kind::Npc)]);
	return selected;
}

std::optional<Cue> Tracker::Update(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities,
	uint64_t now, const Settings& settings) {
	if (played_ && now < last_) Reset();
	const auto selected = Select(world, entities, settings);
	std::array<bool, SlotCount> present{};
	for (const auto& cue : selected) present[cue.slot] = true;
	for (size_t i = 0; i < slots_.size(); ++i) {
		if (!present[i]) slots_[i] = {};
	}
	if (played_ && now - last_ < settings.gapMs) return std::nullopt;
	std::optional<Cue> next;
	double mostOverdue = -1;
	for (const auto& cue : selected) {
		const auto& slot = slots_[cue.slot];
		const auto elapsed = now - slot.last;
		if (slot.played && elapsed < cue.intervalMs) continue;
		// Pending first cues run before repeats. Among repeats, overdue fraction
		// prevents a nearby boss from starving the other selected classes.
		const double overdue = slot.played ? static_cast<double>(elapsed) / std::max<uint64_t>(1, cue.intervalMs)
			: std::numeric_limits<double>::infinity();
		if (!next || overdue > mostOverdue) { next = cue; mostOverdue = overdue; }
	}
	if (next) {
		auto& slot = slots_[next->slot];
		slot.played = true; slot.last = now;
		played_ = true; last_ = now;
	}
	return next;
}
void Tracker::Reset() { slots_ = {}; played_ = false; last_ = 0; }

std::vector<Cue> AlertTracker::Raised(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities) {
	std::vector<Cue> raised;
	for (const auto& entity : entities) {
		if (entity.kind != EntityRadar::Kind::Npc) continue;
		const auto known = std::find(shown_.begin(), shown_.end(), entity.id);
		// An NPC out of this read keeps its place: only one seen without the mark is forgotten.
		if (!entity.npcAlert) { if (known != shown_.end()) shown_.erase(known); continue; }
		if (known != shown_.end()) continue;
		shown_.push_back(entity.id);
		const auto bearing = BearingOf(world.player, entity.position);
		raised.push_back(Cue{entity.id, Kind::Npc, false, bearing.distance, bearing.right, bearing.front});
	}
	std::sort(raised.begin(), raised.end(), Nearer);
	return raised;
}
}
