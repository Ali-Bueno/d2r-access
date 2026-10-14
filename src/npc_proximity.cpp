#include "npc_proximity.h"
#include "spoken_direction.h"

#include <cmath>

namespace D2RAccess::NpcProximity {
std::optional<Announcement> Tracker::Update(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities, const Settings& settings) {
	std::map<uint32_t, std::pair<const EntityRadar::Entity*, double>> present;
	for (const auto& entity : entities) {
		if (entity.kind != EntityRadar::Kind::Npc) continue;
		const double distance = std::hypot(entity.position.x - world.player.x, entity.position.y - world.player.y);
		if (std::isfinite(distance)) present.emplace(entity.id, std::make_pair(&entity, distance));
	}
	// Release what left (beyond range plus margin) or vanished; admit what entered.
	for (auto it = inRange_.begin(); it != inRange_.end();) {
		const auto found = present.find(it->first);
		if (found == present.end() || found->second.second > settings.range + settings.releaseMargin) it = inRange_.erase(it);
		else ++it;
	}
	for (const auto& [id, candidate] : present) {
		if (inRange_.contains(id) || candidate.second > settings.range) continue;
		if (settings.requireLineOfSight && !EntityRadar::LineOfSight(world, world.player, candidate.first->position)) continue;
		inRange_.emplace(id, false);
	}
	// The nearest remembered NPC speaks once; ties by id like the scan.
	const std::pair<const EntityRadar::Entity*, double>* nearest = nullptr;
	uint32_t nearestId = 0;
	for (const auto& [id, announced] : inRange_) {
		const auto& candidate = present.at(id);
		if (!nearest || candidate.second < nearest->second || (candidate.second == nearest->second && id < nearestId)) { nearest = &candidate; nearestId = id; }
	}
	if (!nearest || inRange_[nearestId]) return std::nullopt;
	inRange_[nearestId] = true;
	const auto& position = nearest->first->position;
	return Announcement{*nearest->first, nearest->second, {position.x - world.player.x, position.y - world.player.y}};
}

void Tracker::Reset() { inRange_.clear(); }

std::string Describe(const Announcement& announcement, const EntityRadar::Settings& scan, const Strings::Table& strings) {
	std::string text = announcement.entity.name + ", " + EntityRadar::DistancePhrase(announcement.distance, scan, strings);
	if (!EntityRadar::UnderPlayer(announcement.distance, scan))
		text += std::string(", ") + SpokenDirection::Of(announcement.offset, strings);
	return text;
}
}
