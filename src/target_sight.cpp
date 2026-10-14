#include "target_sight.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace D2RAccess::TargetSight {
namespace {
// Policy: without the game's own target a shared label is one unit's when the
// next bearer is this much further than the nearest: the five Cairn Stones stand
// about eight units apart, so a stone stood beside is told from its neighbours.
constexpr double DistinctUnits = 4;
bool Boundary(unsigned char c) { return c < 128 && !std::isalnum(c); }
const std::string& WorldName(const EntityRadar::Entity& entity) { return entity.worldName.empty() ? entity.name : entity.worldName; }
// What the scan calls `entity` beyond the game's label, if anything.
void Name(Bearer& bearer, const EntityRadar::Entity& entity) {
	if (entity.worldName.empty() || entity.worldName == entity.name) return;
	bearer.name = entity.name;
	bearer.worldName = entity.worldName;
}
}

bool Names(std::string_view label, std::string_view name) {
	if (name.empty() || label.empty()) return false;
	for (size_t pos = label.find(name); pos != std::string_view::npos; pos = label.find(name, pos + 1)) {
		const bool left = pos == 0 || Boundary(static_cast<unsigned char>(label[pos - 1]));
		const size_t after = pos + name.size();
		const bool right = after == label.size() || Boundary(static_cast<unsigned char>(label[after]));
		if (left && right) return true;
	}
	return false;
}

Bearer Find(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities,
	std::string_view label, const Settings& settings, const std::optional<Aim>& aim) {
	if (!settings.enabled || label.empty()) return {};
	bool bearer = false;
	Bearer nearest;
	const EntityRadar::Entity* named = nullptr;
	double next = std::numeric_limits<double>::infinity(); // the second nearest bearer, in sight or not
	for (const auto& entity : entities) {
		// Items are announced by their own proximity service, which decides on
		// distance rather than sight; a level exit is a fixed landmark the scan
		// itself exempts from walls.
		if (entity.kind == EntityRadar::Kind::Item || entity.kind == EntityRadar::Kind::Exit) continue;
		if (!Names(label, WorldName(entity))) continue;
		const double distance = std::hypot(entity.position.x - world.player.x, entity.position.y - world.player.y);
		if (!std::isfinite(distance) || distance > settings.reach) continue;
		bearer = true;
		const bool aimed = aim && entity.unit == aim->unit && entity.id == aim->id;
		if (aimed && EntityRadar::LineOfSight(world, world.player, entity)) {
			Bearer target{Sight::Visible, entity.kind, entity.enemy.important || entity.enemy.boss, distance, entity.id};
			Name(target, entity);
			return target;
		}
		if (nearest.sight == Sight::Visible && distance >= nearest.distance) { next = std::min(next, distance); continue; }
		if (!EntityRadar::LineOfSight(world, world.player, entity)) { next = std::min(next, distance); continue; }
		if (nearest.sight == Sight::Visible) next = std::min(next, nearest.distance);
		nearest = {Sight::Visible, entity.kind, entity.enemy.important || entity.enemy.boss, distance, entity.id};
		named = &entity;
	}
	// Every bearer of that name within reach is behind a wall: the game is
	// naming something the player cannot see.
	if (nearest.sight != Sight::Visible) return {bearer ? Sight::Hidden : Sight::Unknown};
	if (next - nearest.distance >= DistinctUnits) Name(nearest, *named);
	return nearest;
}

std::string Spoken(std::string_view label, const Bearer& bearer) {
	std::string spoken(label);
	if (bearer.name.empty()) return spoken;
	for (size_t pos = spoken.find(bearer.worldName); pos != std::string::npos; pos = spoken.find(bearer.worldName, pos + 1)) {
		const size_t after = pos + bearer.worldName.size();
		if ((pos && !Boundary(static_cast<unsigned char>(spoken[pos - 1])))
			|| (after < spoken.size() && !Boundary(static_cast<unsigned char>(spoken[after])))) continue;
		return spoken.replace(pos, bearer.worldName.size(), bearer.name);
	}
	return spoken;
}

Sight Of(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities,
	std::string_view label, const Settings& settings) {
	return Find(world, entities, label, settings).sight;
}
}
